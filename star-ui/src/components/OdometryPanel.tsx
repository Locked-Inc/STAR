import { useEffect, useRef } from 'react';
import type { CSSProperties } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import type { OdometryData } from '../proto/star/v1/ui';

const TRAIL_MAX = 200;
const CANVAS_WIDTH = 260;
const CANVAS_HEIGHT = 120;

const PIXELS_PER_METER = 20;
const ROBOT_MARKER_RADIUS_PX = 4;
const HEADING_ARROW_LENGTH_PX = 12;
const GRID_CELLS = 6;

const CANVAS_STYLE: CSSProperties = {
  background: 'transparent',
  borderRadius: '12px',
  border: '0.5px solid rgba(255, 255, 255, 0.1)',
  display: 'block',
  boxShadow: 'inset 0 4px 12px rgba(0,0,0,0.3)',
};

export function OdometryPanel() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const trailRef = useRef<{ x: number[]; y: number[] }>({ x: [], y: [] });
  const odometry = useDashboardStore(useShallow((s) => s.odometry));

  useEffect(() => {
    if (!odometry) return;

    // Accumulate trail
    const trail = trailRef.current;
    trail.x.push(odometry.xM);
    trail.y.push(odometry.yM);
    if (trail.x.length > TRAIL_MAX) {
      trail.x.shift();
      trail.y.shift();
    }

    drawOdometry(canvasRef.current, odometry, trail.x, trail.y);
  }, [odometry]);

  useEffect(() => {
    return () => { trailRef.current = { x: [], y: [] }; };
  }, []);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div
        className="panel-header"
        style={{
          padding: '16px 24px 12px 24px',
          fontSize: '11px',
          fontWeight: 600,
          color: 'rgba(255, 255, 255, 0.5)',
          textTransform: 'uppercase',
          letterSpacing: '0.12em',
          userSelect: 'none',
        }}
      >
        Odometry
      </div>
      <div className="panel-body" style={{ padding: '4px 24px 24px 24px', display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
        <canvas
          ref={canvasRef}
          width={CANVAS_WIDTH}
          height={CANVAS_HEIGHT}
          style={CANVAS_STYLE}
        />
        {odometry && (
          <div style={{ display: 'flex', gap: '32px', marginTop: '16px', justifyContent: 'center', width: '100%' }}>
            <OdomStat label="X" value={odometry.xM} unit="m" />
            <OdomStat label="Y" value={odometry.yM} unit="m" />
            <OdomStat label="Heading" value={odometry.thetaRad} unit="rad" />
          </div>
        )}
      </div>
    </div>
  );
}

function OdomStat({ label, value, unit }: { label: string; value: number; unit: string }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '4px' }}>
      <span style={{ fontSize: '10px', color: 'rgba(255,255,255,0.5)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>{label}</span>
      <div style={{ display: 'flex', alignItems: 'baseline', gap: '2px' }}>
        <span className="data-flash" key={value} style={{ fontSize: '16px', fontVariantNumeric: 'tabular-nums', fontWeight: 300, color: '#fff' }}>
          {value.toFixed(2)}
        </span>
        <span style={{ fontSize: '10px', color: 'rgba(255,255,255,0.4)' }}>{unit}</span>
      </div>
    </div>
  );
}

function drawOdometry(
  canvas: HTMLCanvasElement | null,
  odom: OdometryData,
  trailX: number[],
  trailY: number[],
): void {
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;

  const w = canvas.width;
  const h = canvas.height;
  const cx = w / 2;
  const cy = h / 2;

  ctx.clearRect(0, 0, w, h);

  // Faint Grid lines matching strict Liquid Glass aesthetics
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.06)';
  ctx.lineWidth = 1;
  for (let i = -GRID_CELLS; i <= GRID_CELLS; i++) {
    const x = cx + i * PIXELS_PER_METER;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
    const y = cy + i * PIXELS_PER_METER;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }

  // Fading Trail
  if (trailX.length > 1) {
    ctx.lineWidth = 2.5;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';

    for (let i = 1; i < trailX.length; i++) {
      ctx.beginPath();
      ctx.moveTo(cx + trailX[i - 1] * PIXELS_PER_METER, cy - trailY[i - 1] * PIXELS_PER_METER);
      ctx.lineTo(cx + trailX[i] * PIXELS_PER_METER, cy - trailY[i] * PIXELS_PER_METER);

      // Calculate fade based on position in trail. Newer = brighter.
      const alpha = Math.max(0.05, 0.8 * (i / trailX.length));
      ctx.strokeStyle = `rgba(56, 189, 248, ${alpha})`;
      ctx.stroke();
    }
  }

  // Robot Position Marker
  const rx = cx + odom.xM * PIXELS_PER_METER;
  const ry = cy - odom.yM * PIXELS_PER_METER;

  ctx.fillStyle = '#38bdf8';
  ctx.shadowColor = 'rgba(56, 189, 248, 0.6)';
  ctx.shadowBlur = 8;
  ctx.beginPath();
  ctx.arc(rx, ry, ROBOT_MARKER_RADIUS_PX, 0, Math.PI * 2);
  ctx.fill();

  // Reset shadow for arrow
  ctx.shadowBlur = 0;

  // Heading Arrow
  ctx.strokeStyle = '#4ade80'; // Neon green
  ctx.lineCap = 'round';
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  ctx.moveTo(rx, ry);
  ctx.lineTo(
    rx + Math.cos(odom.thetaRad) * HEADING_ARROW_LENGTH_PX,
    ry - Math.sin(odom.thetaRad) * HEADING_ARROW_LENGTH_PX,
  );
  ctx.stroke();
}
