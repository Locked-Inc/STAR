import { useEffect, useRef } from 'react';
import { useDashboardStore } from '../store/dashboardStore';
import type { OdometryData } from '../proto/star/v1/ui';

const TRAIL_MAX = 200;

// Trail stored outside React state
const trailX: number[] = [];
const trailY: number[] = [];

export function OdometryPanel() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const odometry = useDashboardStore((s) => s.odometry);

  useEffect(() => {
    if (!odometry) return;
    drawOdometry(canvasRef.current, odometry);

    // Accumulate trail
    trailX.push(odometry.xM);
    trailY.push(odometry.yM);
    if (trailX.length > TRAIL_MAX) {
      trailX.shift();
      trailY.shift();
    }
  }, [odometry]);

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
        Odometry
      </div>
      <div style={{ padding: '8px' }}>
        <canvas
          ref={canvasRef}
          width={260}
          height={200}
          style={{ background: '#0f1117', borderRadius: '4px', display: 'block' }}
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

function drawOdometry(canvas: HTMLCanvasElement | null, odom: OdometryData): void {
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;

  const w = canvas.width;
  const h = canvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const scale = 40; // pixels per meter

  ctx.clearRect(0, 0, w, h);

  // Grid lines
  ctx.strokeStyle = '#1e2335';
  ctx.lineWidth = 1;
  for (let i = -5; i <= 5; i++) {
    const x = cx + i * scale;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
    const y = cy + i * scale;
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
    ctx.moveTo(cx + trailX[0] * scale, cy - trailY[0] * scale);
    for (let i = 1; i < trailX.length; i++) {
      ctx.lineTo(cx + trailX[i] * scale, cy - trailY[i] * scale);
    }
    ctx.stroke();
  }

  // Robot position
  const rx = cx + odom.xM * scale;
  const ry = cy - odom.yM * scale;

  ctx.fillStyle = '#3b82f6';
  ctx.beginPath();
  ctx.arc(rx, ry, 5, 0, Math.PI * 2);
  ctx.fill();

  // Heading arrow
  const arrowLen = 15;
  ctx.strokeStyle = '#22c55e';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(rx, ry);
  ctx.lineTo(
    rx + Math.cos(odom.thetaRad) * arrowLen,
    ry - Math.sin(odom.thetaRad) * arrowLen,
  );
  ctx.stroke();
}
