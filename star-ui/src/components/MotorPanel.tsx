import { useEffect, useRef } from 'react';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';
import { useDashboardStore } from '../store/dashboardStore';
import type { MotorStatus } from '../proto/star/v1/motor_control';

const WINDOW_PTS = 300; // 30 seconds @ 10 Hz

// Float64Arrays live OUTSIDE React state -- no GC pressure per tick.
const timestamps = new Float64Array(WINDOW_PTS);
const flVelocity = new Float64Array(WINDOW_PTS);
const frVelocity = new Float64Array(WINDOW_PTS);
const blVelocity = new Float64Array(WINDOW_PTS);
const brVelocity = new Float64Array(WINDOW_PTS);

// Pre-allocated scratch arrays -- one per series
const scratchTs = new Float64Array(WINDOW_PTS);
const scratchFL = new Float64Array(WINDOW_PTS);
const scratchFR = new Float64Array(WINDOW_PTS);
const scratchBL = new Float64Array(WINDOW_PTS);
const scratchBR = new Float64Array(WINDOW_PTS);

let writeIdx = 0;

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

  useEffect(() => {
    const container = containerRef.current;
    if (!container) return;

    const opts: uPlot.Options = {
      width: container.offsetWidth || 400,
      height: 180,
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
      [scratchTs, scratchFL, scratchFR, scratchBL, scratchBR],
      container,
    );
    chartRef.current = u;

    // Subscribe to motors via Zustand .subscribe() -- imperative, no React re-render
    const unsub = useDashboardStore.subscribe(
      (s) => s.motors,
      (motors: MotorStatus[] | null) => {
        if (!motors) return;
        timestamps[writeIdx] = Date.now() / 1000;
        flVelocity[writeIdx] = motors[0]?.velocityMps ?? 0;
        frVelocity[writeIdx] = motors[1]?.velocityMps ?? 0;
        blVelocity[writeIdx] = motors[2]?.velocityMps ?? 0;
        brVelocity[writeIdx] = motors[3]?.velocityMps ?? 0;
        writeIdx = (writeIdx + 1) % WINDOW_PTS;

        if (chartRef.current) {
          chartRef.current.setData([
            rolledView(timestamps, writeIdx, scratchTs),
            rolledView(flVelocity, writeIdx, scratchFL),
            rolledView(frVelocity, writeIdx, scratchFR),
            rolledView(blVelocity, writeIdx, scratchBL),
            rolledView(brVelocity, writeIdx, scratchBR),
          ]);
        }
      },
    );

    return () => {
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
