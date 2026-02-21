import { useEffect, useRef } from 'react';
import type { CSSProperties } from 'react';
import { useDashboardStore } from '../store/dashboardStore';
import type { LidarScan } from '../proto/star/v1/ui';
import { PANEL_CONTAINER_STYLE, PANEL_HEADER_STYLE } from '../theme';

const CANVAS_SIZE_PX = 300;
const PIXELS_PER_METER = 80;
const MAX_RANGE_M = 12;
const INTENSITY_DIVISOR = 255;
const RING_STEP = 2;
const RING_START = 1;
const POINT_HALF_WIDTH = 1;    // fillRect uses half-width
const POINT_SIZE = POINT_HALF_WIDTH * 2;
const RING_STROKE_ALPHA = 0.05;

const CONTAINER_STYLE: CSSProperties = { ...PANEL_CONTAINER_STYLE };
const HEADER_STYLE: CSSProperties = { ...PANEL_HEADER_STYLE };
const CONTENT_STYLE: CSSProperties = { padding: '8px' };
const CANVAS_STYLE: CSSProperties = { background: '#0a0c14', borderRadius: '4px', display: 'block' };

export function LidarPanel() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    // Draw any scan already in the store before subscribing
    const initialScan = useDashboardStore.getState().lidarScan;
    if (
      initialScan &&
      initialScan.angleRad.length === initialScan.rangeM.length &&
      initialScan.angleRad.length === initialScan.intensity.length
    ) {
      drawScan(canvasRef.current, initialScan);
    }

    // Subscribe to Zustand without triggering React re-renders
    const unsub = useDashboardStore.subscribe(
      (state) => state.lidarScan,
      (scan: LidarScan | null) => {
        // Validate SoA invariant before drawing
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
    <div style={CONTAINER_STYLE}>
      <div style={HEADER_STYLE}>
        LiDAR
      </div>
      <div style={CONTENT_STYLE}>
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

  // Draw range rings for reference
  ctx.strokeStyle = `rgba(255, 255, 255, ${RING_STROKE_ALPHA})`;
  ctx.lineWidth = 1;
  for (let r = RING_START; r <= MAX_RANGE_M; r += RING_STEP) {
    ctx.beginPath();
    ctx.arc(cx, cy, r * PIXELS_PER_METER, 0, Math.PI * 2);
    ctx.stroke();
  }

  // Draw points
  for (let i = 0; i < scan.angleRad.length; i++) {
    const d = scan.rangeM[i];
    const a = scan.angleRad[i];
    const q = Math.max(0, Math.min(1, scan.intensity[i] / INTENSITY_DIVISOR));
    if (d <= 0 || d > MAX_RANGE_M) continue; // filter invalid points

    const x = cx + Math.cos(a) * d * PIXELS_PER_METER;
    const y = cy - Math.sin(a) * d * PIXELS_PER_METER;

    ctx.fillStyle = `rgba(0, 220, 80, ${q.toFixed(2)})`;
    ctx.fillRect(x - POINT_HALF_WIDTH, y - POINT_HALF_WIDTH, POINT_SIZE, POINT_SIZE);
  }

  // Robot center
  ctx.fillStyle = '#3b82f6';
  ctx.beginPath();
  ctx.arc(cx, cy, 3, 0, Math.PI * 2);
  ctx.fill();
}
