import { useEffect, useRef } from 'react';
import type { CSSProperties } from 'react';
import { useDashboardStore } from '../store/dashboardStore';
import type { LidarScan } from '../proto/star/v1/ui';

const CANVAS_SIZE_PX = 200;
const PIXELS_PER_METER = 8;
const MAX_RANGE_M = 12;
const INTENSITY_DIVISOR = 255;
const POINT_RADIUS = 1.5;

const CANVAS_STYLE: CSSProperties = {
  background: '#060c18', // Deep navy background as requested
  borderRadius: '50%', // Circle canvas mask
  border: '0.5px solid rgba(255, 255, 255, 0.15)',
  display: 'block',
  boxShadow: 'inset 0 4px 12px rgba(0,0,0,0.4), 0 0 20px rgba(0,229,255,0.05)',
};

export function LidarPanel() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const initialScan = useDashboardStore.getState().lidarScan;
    if (
      initialScan &&
      initialScan.angleRad.length === initialScan.rangeM.length &&
      initialScan.angleRad.length === initialScan.intensity.length
    ) {
      drawScan(canvasRef.current, initialScan);
    }

    const unsub = useDashboardStore.subscribe(
      (state) => state.lidarScan,
      (scan: LidarScan | null) => {
        if (
          scan &&
          scan.angleRad.length === scan.rangeM.length &&
          scan.angleRad.length === scan.intensity.length
        ) {
          drawScan(canvasRef.current, scan);
        }
      },
    );
    return unsub;
  }, []);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', alignItems: 'center' }}>
      <div
        className="panel-header"
        style={{
          width: '100%',
          padding: '16px 24px 8px 24px',
          fontSize: '11px',
          fontWeight: 600,
          color: 'rgba(255, 255, 255, 0.5)',
          textTransform: 'uppercase',
          letterSpacing: '0.12em',
          userSelect: 'none',
        }}
      >
        LiDAR Radar
      </div>
      <div className="panel-body" style={{ padding: '8px 24px 24px 24px', display: 'flex', justifyContent: 'center', alignItems: 'center', width: '100%' }}>
        <canvas
          ref={canvasRef}
          width={CANVAS_SIZE_PX}
          height={CANVAS_SIZE_PX}
          style={CANVAS_STYLE}
        />
      </div>
    </div>
  );
}

function drawScan(canvas: HTMLCanvasElement | null, scan: LidarScan): void {
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;

  const cx = canvas.width / 2;
  const cy = canvas.height / 2;

  ctx.clearRect(0, 0, canvas.width, canvas.height);

  // Faint dashed max-range ring
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 4]); // Dashed
  ctx.beginPath();
  ctx.arc(cx, cy, MAX_RANGE_M * PIXELS_PER_METER, 0, Math.PI * 2);
  ctx.stroke();

  // Mid range ring
  ctx.beginPath();
  ctx.arc(cx, cy, (MAX_RANGE_M / 2) * PIXELS_PER_METER, 0, Math.PI * 2);
  ctx.stroke();

  ctx.setLineDash([]); // Reset dash for regular lines

  ctx.fillStyle = '#00e5ff'; // Cyan point color

  // Draw points
  for (let i = 0; i < scan.angleRad.length; i++) {
    const d = scan.rangeM[i];
    const a = scan.angleRad[i];
    const q = Math.max(0.1, Math.min(1, scan.intensity[i] / INTENSITY_DIVISOR)); // Ensure min opacity
    if (d <= 0 || d > MAX_RANGE_M) continue; // filter invalid points

    const x = cx + Math.cos(a) * d * PIXELS_PER_METER;
    const y = cy - Math.sin(a) * d * PIXELS_PER_METER;

    ctx.globalAlpha = q;
    ctx.beginPath();
    ctx.arc(x, y, POINT_RADIUS, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.globalAlpha = 1.0;

  // Drawn triangular robot at center pointing upward (angle 0 goes right usually, we will just point it 'forward')
  ctx.fillStyle = '#4ade80'; // Neon green robot arrow
  ctx.shadowColor = 'rgba(74, 222, 128, 0.6)';
  ctx.shadowBlur = 6;
  const triangleSize = 5;

  ctx.beginPath();
  // Math: 0 rad usually goes Right. For standard robotics, X is forward, Y is left.
  // Assuming robot is pointing right on the canvas. So nose is (triangleSize, 0)
  ctx.moveTo(cx + triangleSize, cy);
  ctx.lineTo(cx - triangleSize, cy - triangleSize);
  ctx.lineTo(cx - triangleSize, cy + triangleSize);
  ctx.closePath();
  ctx.fill();

  ctx.shadowBlur = 0;
}
