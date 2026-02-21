import { useEffect, useRef } from 'react';
import type { CSSProperties } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import type { OdometryData } from '../proto/star/v1/ui';
import { PANEL_CONTAINER_STYLE, PANEL_HEADER_STYLE } from '../theme';

const TRAIL_MAX = 200;
const CANVAS_WIDTH = 260;
const CANVAS_HEIGHT = 200;

const PIXELS_PER_METER = 40;
const ROBOT_MARKER_RADIUS_PX = 5;
const HEADING_ARROW_LENGTH_PX = 15;
const GRID_CELLS = 5;

const CONTAINER_STYLE: CSSProperties = { ...PANEL_CONTAINER_STYLE };
const HEADER_STYLE: CSSProperties = { ...PANEL_HEADER_STYLE };
const CONTENT_STYLE: CSSProperties = { padding: '8px' };
const CANVAS_STYLE: CSSProperties = { background: '#0f1117', borderRadius: '4px', display: 'block' };

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
    <div style={CONTAINER_STYLE}>
      <div style={HEADER_STYLE}>
        Odometry
      </div>
      <div style={CONTENT_STYLE}>
        <canvas
          ref={canvasRef}
          width={CANVAS_WIDTH}
          height={CANVAS_HEIGHT}
          style={CANVAS_STYLE}
        />
        {odometry && (
          <div style={{ marginTop: '6px', fontSize: '11px', color: '#9ca3af', fontFamily: 'monospace' }}>
            x={odometry.xM.toFixed(2)} y={odometry.yM.toFixed(2)}{' '}
            th={odometry.thetaRad.toFixed(2)} rad
          </div>
        )}
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

  // Grid lines
  ctx.strokeStyle = '#1e2335';
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

  // Trail
  if (trailX.length > 1) {
    ctx.strokeStyle = 'rgba(59, 130, 246, 0.5)';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(cx + trailX[0] * PIXELS_PER_METER, cy - trailY[0] * PIXELS_PER_METER);
    for (let i = 1; i < trailX.length; i++) {
      ctx.lineTo(cx + trailX[i] * PIXELS_PER_METER, cy - trailY[i] * PIXELS_PER_METER);
    }
    ctx.stroke();
  }

  // Robot position
  const rx = cx + odom.xM * PIXELS_PER_METER;
  const ry = cy - odom.yM * PIXELS_PER_METER;

  ctx.fillStyle = '#3b82f6';
  ctx.beginPath();
  ctx.arc(rx, ry, ROBOT_MARKER_RADIUS_PX, 0, Math.PI * 2);
  ctx.fill();

  // Heading arrow
  ctx.strokeStyle = '#22c55e';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(rx, ry);
  ctx.lineTo(
    rx + Math.cos(odom.thetaRad) * HEADING_ARROW_LENGTH_PX,
    ry - Math.sin(odom.thetaRad) * HEADING_ARROW_LENGTH_PX,
  );
  ctx.stroke();
}
