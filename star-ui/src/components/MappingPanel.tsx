import { useEffect, useRef, useCallback } from 'react';
import type { CSSProperties } from 'react';
import { useDashboardStore } from '../store/dashboardStore';
import type { LidarScan } from '../proto/star/v1/ui';
import { PANEL_CONTAINER_STYLE, PANEL_HEADER_STYLE, COLORS } from '../theme';

// ── Scan view constants (matches LidarPanel) ──────────────────────────────
const SCAN_SIZE_PX = 300;
const SCAN_PX_PER_M = 80;
const SCAN_MAX_RANGE_M = 12;
const SCAN_RING_STEP = 2;
const SCAN_RING_ALPHA = 0.05;
const SCAN_INTENSITY_DIV = 255;

// ── Map view constants ────────────────────────────────────────────────────
const MAP_SIZE_PX = 500;
const MAP_INITIAL_PX_PER_M = 60; // pixels per meter; rescaled when extent grows
const MAP_POINT_BUFFER_MAX = 60_000;
const MAP_ROBOT_RADIUS_PX = 5;
const MAP_ARROW_LEN_PX = 14;
const MAP_GRID_SPACING_M = 1.0; // draw grid every 1 m

// ── Styles ────────────────────────────────────────────────────────────────
const OUTER_STYLE: CSSProperties = {
  ...PANEL_CONTAINER_STYLE,
  width: '100%',
};

const HEADER_STYLE: CSSProperties = { ...PANEL_HEADER_STYLE };

const PANELS_ROW: CSSProperties = {
  display: 'flex',
  gap: '8px',
  padding: '8px',
  flexWrap: 'wrap',
};

const SUB_PANEL: CSSProperties = {
  background: COLORS.bodyBg,
  border: `1px solid ${COLORS.border}`,
  borderRadius: '4px',
  padding: '6px',
};

const SUB_HEADER: CSSProperties = {
  fontSize: '10px',
  fontWeight: 'bold',
  color: COLORS.textDim,
  textTransform: 'uppercase',
  letterSpacing: '0.05em',
  marginBottom: '4px',
};

const CANVAS_STYLE: CSSProperties = {
  background: '#0a0c14',
  borderRadius: '4px',
  display: 'block',
};

const FOOTER_STYLE: CSSProperties = {
  display: 'flex',
  alignItems: 'center',
  gap: '12px',
  padding: '6px 8px',
  borderTop: `1px solid ${COLORS.border}`,
  fontSize: '11px',
  color: COLORS.textMuted,
};

const RESET_BTN_STYLE: CSSProperties = {
  padding: '4px 12px',
  background: COLORS.accent,
  color: '#fff',
  border: 'none',
  borderRadius: '4px',
  fontSize: '11px',
  fontWeight: 'bold',
  cursor: 'pointer',
};

// ── Types ─────────────────────────────────────────────────────────────────
interface WorldPoint { x: number; y: number; }

// ── Component ─────────────────────────────────────────────────────────────
export function MappingPanel() {
  const scanCanvasRef = useRef<HTMLCanvasElement>(null);
  const mapCanvasRef = useRef<HTMLCanvasElement>(null);
  const pointBuffer = useRef<WorldPoint[]>([]);
  const pointCountRef = useRef(0); // tracks length for footer display (no re-render)

  // ── Draw the current scan (polar, same as LidarPanel) ──────────────────
  const drawScan = useCallback((scan: LidarScan) => {
    const canvas = scanCanvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const cx = SCAN_SIZE_PX / 2;
    const cy = SCAN_SIZE_PX / 2;

    ctx.clearRect(0, 0, SCAN_SIZE_PX, SCAN_SIZE_PX);

    // Range rings
    ctx.strokeStyle = `rgba(255,255,255,${SCAN_RING_ALPHA})`;
    ctx.lineWidth = 1;
    for (let r = SCAN_RING_STEP; r <= SCAN_MAX_RANGE_M; r += SCAN_RING_STEP) {
      ctx.beginPath();
      ctx.arc(cx, cy, r * SCAN_PX_PER_M, 0, Math.PI * 2);
      ctx.stroke();
    }

    // Points
    for (let i = 0; i < scan.angleRad.length; i++) {
      const d = scan.rangeM[i];
      const a = scan.angleRad[i];
      const q = Math.max(0, Math.min(1, scan.intensity[i] / SCAN_INTENSITY_DIV));
      if (d <= 0 || d > SCAN_MAX_RANGE_M) continue;
      const x = cx + Math.cos(a) * d * SCAN_PX_PER_M;
      const y = cy - Math.sin(a) * d * SCAN_PX_PER_M;
      ctx.fillStyle = `rgba(0,220,80,${q.toFixed(2)})`;
      ctx.fillRect(x - 1, y - 1, 2, 2);
    }

    // Robot center
    ctx.fillStyle = '#3b82f6';
    ctx.beginPath();
    ctx.arc(cx, cy, 3, 0, Math.PI * 2);
    ctx.fill();
  }, []);

  // ── Draw the accumulated map ────────────────────────────────────────────
  const drawMap = useCallback(() => {
    const canvas = mapCanvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const pts = pointBuffer.current;
    const odom = useDashboardStore.getState().odometry;

    ctx.clearRect(0, 0, MAP_SIZE_PX, MAP_SIZE_PX);

    if (pts.length === 0 && !odom) return;

    // Compute world extent to auto-scale
    let minX = -2, maxX = 2, minY = -2, maxY = 2;
    if (odom) {
      minX = Math.min(minX, odom.xM);
      maxX = Math.max(maxX, odom.xM);
      minY = Math.min(minY, odom.yM);
      maxY = Math.max(maxY, odom.yM);
    }
    for (const p of pts) {
      if (p.x < minX) minX = p.x;
      if (p.x > maxX) maxX = p.x;
      if (p.y < minY) minY = p.y;
      if (p.y > maxY) maxY = p.y;
    }

    const padding = 1.0; // metres of margin around extent
    const worldW = maxX - minX + padding * 2;
    const worldH = maxY - minY + padding * 2;
    const scale = Math.min(
      MAP_SIZE_PX / worldW,
      MAP_SIZE_PX / worldH,
      MAP_INITIAL_PX_PER_M,
    );

    // World → canvas transform (Y axis flipped: world +Y = canvas up)
    const toCanvasX = (wx: number) => (wx - minX + padding) * scale;
    const toCanvasY = (wy: number) => MAP_SIZE_PX - (wy - minY + padding) * scale;

    // Grid lines every 1 m
    ctx.strokeStyle = '#1e2335';
    ctx.lineWidth = 1;
    const gridStart = Math.floor(minX - padding);
    const gridEndX = Math.ceil(maxX + padding);
    const gridEndY = Math.ceil(maxY + padding);
    for (let gx = gridStart; gx <= gridEndX; gx += MAP_GRID_SPACING_M) {
      const cx = toCanvasX(gx);
      ctx.beginPath();
      ctx.moveTo(cx, 0);
      ctx.lineTo(cx, MAP_SIZE_PX);
      ctx.stroke();
    }
    const gridStartY = Math.floor(minY - padding);
    for (let gy = gridStartY; gy <= gridEndY; gy += MAP_GRID_SPACING_M) {
      const cy = toCanvasY(gy);
      ctx.beginPath();
      ctx.moveTo(0, cy);
      ctx.lineTo(MAP_SIZE_PX, cy);
      ctx.stroke();
    }

    // Accumulated map points
    ctx.fillStyle = 'rgba(0,220,80,0.8)';
    for (const p of pts) {
      const cx = toCanvasX(p.x);
      const cy = toCanvasY(p.y);
      ctx.fillRect(cx - 1, cy - 1, 2, 2);
    }

    // Robot position
    if (odom) {
      const rx = toCanvasX(odom.xM);
      const ry = toCanvasY(odom.yM);

      ctx.fillStyle = '#3b82f6';
      ctx.beginPath();
      ctx.arc(rx, ry, MAP_ROBOT_RADIUS_PX, 0, Math.PI * 2);
      ctx.fill();

      ctx.strokeStyle = '#22c55e';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(rx, ry);
      ctx.lineTo(
        rx + Math.cos(odom.thetaRad) * MAP_ARROW_LEN_PX,
        ry - Math.sin(odom.thetaRad) * MAP_ARROW_LEN_PX,
      );
      ctx.stroke();
    }
  }, []);

  // ── Accumulate scan points into world frame and redraw ──────────────────
  const accumulateAndDraw = useCallback((scan: LidarScan) => {
    const odom = useDashboardStore.getState().odometry;
    if (!odom) {
      // No pose yet — just redraw scan, skip accumulation
      drawScan(scan);
      return;
    }

    const { xM, yM, thetaRad } = odom;
    const buf = pointBuffer.current;

    for (let i = 0; i < scan.angleRad.length; i++) {
      const d = scan.rangeM[i];
      if (d <= 0 || d > SCAN_MAX_RANGE_M || !Number.isFinite(d)) continue;
      const a = scan.angleRad[i];
      buf.push({
        x: xM + d * Math.cos(thetaRad + a),
        y: yM + d * Math.sin(thetaRad + a),
      });
    }

    // Rolling buffer — drop oldest when over limit
    if (buf.length > MAP_POINT_BUFFER_MAX) {
      buf.splice(0, buf.length - MAP_POINT_BUFFER_MAX);
    }
    pointCountRef.current = buf.length;

    drawScan(scan);
    drawMap();
  }, [drawScan, drawMap]);

  // ── Subscribe (no React re-renders) ────────────────────────────────────
  useEffect(() => {
    const initial = useDashboardStore.getState().lidarScan;
    if (
      initial &&
      initial.angleRad.length === initial.rangeM.length &&
      initial.angleRad.length === initial.intensity.length
    ) {
      accumulateAndDraw(initial);
    }

    const unsub = useDashboardStore.subscribe(
      (state) => state.lidarScan,
      (scan: LidarScan | null) => {
        if (
          scan &&
          scan.angleRad.length === scan.rangeM.length &&
          scan.angleRad.length === scan.intensity.length
        ) {
          accumulateAndDraw(scan);
        }
      },
    );
    return unsub;
  }, [accumulateAndDraw]);

  // ── Reset ───────────────────────────────────────────────────────────────
  const handleReset = useCallback(async () => {
    pointBuffer.current = [];
    pointCountRef.current = 0;
    drawMap(); // clears immediately
    try {
      await fetch('/api/slam/reset', { method: 'POST' });
    } catch {
      // Non-critical — map buffer already cleared client-side
    }
  }, [drawMap]);

  return (
    <div style={OUTER_STYLE}>
      <div style={HEADER_STYLE}>Mapping</div>
      <div style={PANELS_ROW}>
        <div style={SUB_PANEL}>
          <div style={SUB_HEADER}>Current Scan</div>
          <canvas
            ref={scanCanvasRef}
            width={SCAN_SIZE_PX}
            height={SCAN_SIZE_PX}
            style={CANVAS_STYLE}
          />
        </div>
        <div style={SUB_PANEL}>
          <div style={SUB_HEADER}>Accumulated Map</div>
          <canvas
            ref={mapCanvasRef}
            width={MAP_SIZE_PX}
            height={MAP_SIZE_PX}
            style={CANVAS_STYLE}
          />
        </div>
      </div>
      <div style={FOOTER_STYLE}>
        <button style={RESET_BTN_STYLE} onClick={handleReset}>
          Reset Map
        </button>
        <span>Buffer max: {MAP_POINT_BUFFER_MAX.toLocaleString()} pts</span>
      </div>
    </div>
  );
}
