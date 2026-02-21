import { useEffect, useRef } from 'react';
import { useDashboardStore } from '../store/dashboardStore';
import type { LidarScan } from '../proto/star/v1/ui';

export function LidarPanel() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const scanRef = useRef<LidarScan | null>(null);

  useEffect(() => {
    // Subscribe to Zustand without triggering React re-renders
    const unsub = useDashboardStore.subscribe(
      (state) => state.lidarScan,
      (scan: LidarScan | null) => {
        // Validate SoA invariant before storing
        if (
          scan &&
          scan.angleRad.length === scan.rangeM.length &&
          scan.angleRad.length === scan.intensity.length
        ) {
          scanRef.current = scan;
          drawScan(canvasRef.current, scan);
        }
      },
    );
    return unsub;
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
        LiDAR
      </div>
      <div style={{ padding: '8px' }}>
        <canvas
          ref={canvasRef}
          width={300}
          height={300}
          style={{ background: '#0a0c14', borderRadius: '4px', display: 'block' }}
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
  const scale = 80; // pixels per meter

  ctx.clearRect(0, 0, canvas.width, canvas.height);

  // Draw range rings for reference
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
  ctx.lineWidth = 1;
  for (let r = 1; r <= 12; r += 2) {
    ctx.beginPath();
    ctx.arc(cx, cy, r * scale / 2, 0, Math.PI * 2);
    ctx.stroke();
  }

  // Draw points
  for (let i = 0; i < scan.angleRad.length; i++) {
    const d = scan.rangeM[i];
    const a = scan.angleRad[i];
    const q = scan.intensity[i] / 255;
    if (d <= 0 || d > 12) continue; // filter invalid points

    const x = cx + Math.cos(a) * d * scale;
    const y = cy - Math.sin(a) * d * scale;

    ctx.fillStyle = `rgba(0, 220, 80, ${q.toFixed(2)})`;
    ctx.fillRect(x - 1, y - 1, 2, 2);
  }

  // Robot center
  ctx.fillStyle = '#3b82f6';
  ctx.beginPath();
  ctx.arc(cx, cy, 3, 0, Math.PI * 2);
  ctx.fill();
}
