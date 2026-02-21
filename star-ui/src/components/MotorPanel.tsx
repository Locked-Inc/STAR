import { useEffect, useRef } from 'react';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';
import { useDashboardStore } from '../store/dashboardStore';
import type { MotorStatus } from '../proto/star/v1/motor_control';

const WINDOW_PTS = 300; // 30 seconds @ 10 Hz

const MOTOR_IDX_FL = 0;
const MOTOR_IDX_FR = 1;
const MOTOR_IDX_BL = 2;
const MOTOR_IDX_BR = 3;
const EXPECTED_MOTOR_COUNT = 4;

const DEFAULT_CHART_WIDTH = 400;
const DEFAULT_CHART_HEIGHT = 180;

// Float64Arrays live OUTSIDE React state -- no GC pressure per tick.
function createBuffers() {
  return {
    timestamps: new Float64Array(WINDOW_PTS),
    flVelocity: new Float64Array(WINDOW_PTS),
    frVelocity: new Float64Array(WINDOW_PTS),
    blVelocity: new Float64Array(WINDOW_PTS),
    brVelocity: new Float64Array(WINDOW_PTS),
    // Pre-allocated scratch arrays -- one per series
    scratchTs: new Float64Array(WINDOW_PTS),
    scratchFL: new Float64Array(WINDOW_PTS),
    scratchFR: new Float64Array(WINDOW_PTS),
    scratchBL: new Float64Array(WINDOW_PTS),
    scratchBR: new Float64Array(WINDOW_PTS),
    writeIdx: 0,
  };
}

// Unwrap a ring buffer into chronological order, no allocation.
function rolledView(ring: Float64Array, wIdx: number, scratch: Float64Array): Float64Array {
  const tail = ring.subarray(wIdx);       // older half (wIdx -> end)
  const head = ring.subarray(0, wIdx);   // newer half (0 -> wIdx)
  scratch.set(tail, 0);
  scratch.set(head, tail.length);
  return scratch;
}

export function MotorPanel() {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<uPlot | null>(null);
  const buffersRef = useRef<ReturnType<typeof createBuffers> | null>(null);
  if (!buffersRef.current) buffersRef.current = createBuffers();

  useEffect(() => {
    const container = containerRef.current;
    if (!container) return;

    let cancelled = false;
    const buf = buffersRef.current!;

    const opts: uPlot.Options = {
      width: container.offsetWidth || DEFAULT_CHART_WIDTH,
      height: DEFAULT_CHART_HEIGHT,
      series: [
        {},
        { label: 'FL', stroke: '#ef4444' },
        { label: 'FR', stroke: '#3b82f6' },
        { label: 'BL', stroke: '#22c55e' },
        { label: 'BR', stroke: '#f59e0b' },
      ],
      axes: [
        {},
        { label: 'm/s' },
      ],
      scales: {
        x: { time: true },
      },
    };

    const u = new uPlot(
      opts,
      [buf.scratchTs, buf.scratchFL, buf.scratchFR, buf.scratchBL, buf.scratchBR],
      container,
    );
    chartRef.current = u;

    const ro = new ResizeObserver(() => {
      if (chartRef.current && container.offsetWidth > 0) {
        chartRef.current.setSize({ width: container.offsetWidth, height: DEFAULT_CHART_HEIGHT });
      }
    });
    ro.observe(container);

    // Subscribe to motors via Zustand .subscribe() -- imperative, no React re-render
    const unsub = useDashboardStore.subscribe(
      (s) => s.motors,
      (motors: MotorStatus[] | null) => {
        if (cancelled) return;
        if (!motors) return;

        if (motors.length < EXPECTED_MOTOR_COUNT) {
          console.warn(
            `MotorPanel: expected ${EXPECTED_MOTOR_COUNT} motors, got ${motors.length}`,
            motors,
          );
        }

        buf.timestamps[buf.writeIdx] = Date.now() / 1000;
        buf.flVelocity[buf.writeIdx] = motors[MOTOR_IDX_FL]?.velocityMps ?? 0;
        buf.frVelocity[buf.writeIdx] = motors[MOTOR_IDX_FR]?.velocityMps ?? 0;
        buf.blVelocity[buf.writeIdx] = motors[MOTOR_IDX_BL]?.velocityMps ?? 0;
        buf.brVelocity[buf.writeIdx] = motors[MOTOR_IDX_BR]?.velocityMps ?? 0;
        buf.writeIdx = (buf.writeIdx + 1) % WINDOW_PTS;

        if (chartRef.current) {
          chartRef.current.setData([
            rolledView(buf.timestamps, buf.writeIdx, buf.scratchTs),
            rolledView(buf.flVelocity, buf.writeIdx, buf.scratchFL),
            rolledView(buf.frVelocity, buf.writeIdx, buf.scratchFR),
            rolledView(buf.blVelocity, buf.writeIdx, buf.scratchBL),
            rolledView(buf.brVelocity, buf.writeIdx, buf.scratchBR),
          ]);
        }
      },
    );

    return () => {
      cancelled = true;
      ro.disconnect();
      unsub();
      u.destroy();
    };
  }, []);

  return (
    <div
      style={{
        background: '#1a1d27',
        border: '1px solid #2a2e42',
        borderRadius: '6px',
        overflow: 'hidden',
      }}
    >
      <div
        style={{
          padding: '6px 10px',
          borderBottom: '1px solid #2a2e42',
          fontSize: '11px',
          fontWeight: 'bold',
          color: '#9ca3af',
          textTransform: 'uppercase',
          letterSpacing: '0.05em',
        }}
      >
        Motor Velocities
      </div>
      <div ref={containerRef} />
    </div>
  );
}
