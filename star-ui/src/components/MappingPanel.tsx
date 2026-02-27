import { useEffect, useRef, useCallback, useState } from 'react';
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
const SCAN_ROBOT_RADIUS_PX = 3;
const SCAN_COLORS = {
  ring: `rgba(255,255,255,${SCAN_RING_ALPHA})`,
  pointRgb: '0,220,80',
  robot: '#3b82f6',
} as const;

// ── Map view constants ────────────────────────────────────────────────────
const MAP_SIZE_PX = 500;
const MAP_INITIAL_PX_PER_M = 60; // pixels per meter; rescaled when extent grows
const MAP_DEFAULT_EXTENT_M = 2;
const MAP_POINT_BUFFER_MAX = 60_000;
const MAP_ROBOT_RADIUS_PX = 5;
const MAP_ARROW_LEN_PX = 14;
const MAP_GRID_SPACING_M = 1.0; // draw grid every 1 m
const MAP_GRID_COLOR = '#1e2335';
const MAP_POINT_COLOR = 'rgba(0,220,80,0.8)';
const MAP_ROBOT_COLOR = SCAN_COLORS.robot;
const MAP_HEADING_COLOR = '#22c55e';

// ── Rendering constants ───────────────────────────────────────────────────
/** Metres of padding added to each side of the world extent when computing
 *  the canvas scale, so points near the boundary are never clipped. */
const MAP_PADDING_M = 1.0;
/** Stroke width (px) of the heading arrow drawn at the robot position. */
const MAP_HEADING_STROKE_WIDTH_PX = 2;
/** Half-width/height offset used to centre the square drawn for each point;
 *  combined with SCAN_POINT_SIZE_PX this makes a 2x2 px square: fillRect(x-1, y-1, 2, 2). */
const SCAN_POINT_OFFSET_PX = 1;
/** Full width/height (px) of the square drawn for each scan or map point. */
const SCAN_POINT_SIZE_PX = 2;
/** Minimum interval (ms) between point-count React state updates; throttles
 *  re-renders while the ring buffer grows rapidly. */
const POINT_COUNT_THROTTLE_MS = 500;

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
  background: COLORS.bodyBg,
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

interface WorldExtent {
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
  initialized: boolean;
  dirty: boolean;
}

function createEmptyExtent(): WorldExtent {
  return {
    minX: 0,
    maxX: 0,
    minY: 0,
    maxY: 0,
    initialized: false,
    dirty: false,
  };
}

class PointRingBuffer {
  private readonly data: WorldPoint[];
  private readonly capacity: number;
  private head = 0;
  private size = 0;

  constructor(capacity: number) {
    this.capacity = capacity;
    this.data = new Array<WorldPoint>(capacity);
  }

  get length(): number {
    return this.size;
  }

  push(point: WorldPoint): WorldPoint | undefined {
    if (this.size < this.capacity) {
      const idx = (this.head + this.size) % this.capacity;
      this.data[idx] = point;
      this.size += 1;
      return undefined;
    }

    const overwritten = this.data[this.head];
    this.data[this.head] = point;
    this.head = (this.head + 1) % this.capacity;
    return overwritten;
  }

  clear(): void {
    this.head = 0;
    this.size = 0;
  }

  forEach(cb: (point: WorldPoint) => void): void {
    for (let i = 0; i < this.size; i++) {
      cb(this.data[(this.head + i) % this.capacity]);
    }
  }
}

// ── Component ─────────────────────────────────────────────────────────────
export function MappingPanel() {
  const scanCanvasRef = useRef<HTMLCanvasElement>(null);
  const mapCanvasRef = useRef<HTMLCanvasElement>(null);
  const pointBuffer = useRef<PointRingBuffer>(new PointRingBuffer(MAP_POINT_BUFFER_MAX));
  const extentRef = useRef<WorldExtent>(createEmptyExtent());
  const rafRef = useRef<number | null>(null);
  const drawPendingRef = useRef(false);
  const lastPointCountUpdateMsRef = useRef(0);
  const pointCountRef = useRef(0);
  const abortRef = useRef<AbortController | null>(null);
  const [pointCount, setPointCount] = useState(0);

  const includePointInExtent = useCallback((p: WorldPoint) => {
    const extent = extentRef.current;
    if (!extent.initialized) {
      extent.minX = p.x;
      extent.maxX = p.x;
      extent.minY = p.y;
      extent.maxY = p.y;
      extent.initialized = true;
      return;
    }
    if (p.x < extent.minX) extent.minX = p.x;
    if (p.x > extent.maxX) extent.maxX = p.x;
    if (p.y < extent.minY) extent.minY = p.y;
    if (p.y > extent.maxY) extent.maxY = p.y;
  }, []);

  const markExtentDirtyIfNeeded = useCallback((removed: WorldPoint | undefined) => {
    if (!removed) return;
    const extent = extentRef.current;
    if (!extent.initialized) return;
    if (
      removed.x === extent.minX ||
      removed.x === extent.maxX ||
      removed.y === extent.minY ||
      removed.y === extent.maxY
    ) {
      extent.dirty = true;
    }
  }, []);

  const recomputeExtentIfDirty = useCallback(() => {
    const extent = extentRef.current;
    const buf = pointBuffer.current;
    if (!extent.dirty) return;

    if (buf.length === 0) {
      extentRef.current = createEmptyExtent();
      return;
    }

    let initialized = false;
    let minX = 0;
    let maxX = 0;
    let minY = 0;
    let maxY = 0;
    buf.forEach((p) => {
      if (!initialized) {
        minX = p.x;
        maxX = p.x;
        minY = p.y;
        maxY = p.y;
        initialized = true;
        return;
      }
      if (p.x < minX) minX = p.x;
      if (p.x > maxX) maxX = p.x;
      if (p.y < minY) minY = p.y;
      if (p.y > maxY) maxY = p.y;
    });

    extent.minX = minX;
    extent.maxX = maxX;
    extent.minY = minY;
    extent.maxY = maxY;
    extent.initialized = initialized;
    extent.dirty = false;
  }, []);

  const updatePointCountThrottled = useCallback((force = false) => {
    const nowMs = performance.now();
    const len = pointBuffer.current.length;
    if (!force && nowMs - lastPointCountUpdateMsRef.current < POINT_COUNT_THROTTLE_MS) {
      return;
    }
    lastPointCountUpdateMsRef.current = nowMs;
    if (pointCountRef.current !== len) {
      pointCountRef.current = len;
      setPointCount(len);
    }
  }, []);

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
    ctx.strokeStyle = SCAN_COLORS.ring;
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
      ctx.fillStyle = `rgba(${SCAN_COLORS.pointRgb},${q.toFixed(2)})`;
      ctx.fillRect(x - SCAN_POINT_OFFSET_PX, y - SCAN_POINT_OFFSET_PX, SCAN_POINT_SIZE_PX, SCAN_POINT_SIZE_PX);
    }

    // Robot center
    ctx.fillStyle = SCAN_COLORS.robot;
    ctx.beginPath();
    ctx.arc(cx, cy, SCAN_ROBOT_RADIUS_PX, 0, Math.PI * 2);
    ctx.fill();
  }, []);

  // ── Draw the accumulated map ────────────────────────────────────────────
  const drawMap = useCallback(() => {
    const canvas = mapCanvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    recomputeExtentIfDirty();

    const pts = pointBuffer.current;
    const odom = useDashboardStore.getState().odometry;
    const extent = extentRef.current;

    ctx.clearRect(0, 0, MAP_SIZE_PX, MAP_SIZE_PX);

    if (pts.length === 0 && !odom) return;

    // Use incrementally maintained world extent to auto-scale
    let minX = -MAP_DEFAULT_EXTENT_M;
    let maxX = MAP_DEFAULT_EXTENT_M;
    let minY = -MAP_DEFAULT_EXTENT_M;
    let maxY = MAP_DEFAULT_EXTENT_M;
    if (extent.initialized) {
      minX = Math.min(minX, extent.minX);
      maxX = Math.max(maxX, extent.maxX);
      minY = Math.min(minY, extent.minY);
      maxY = Math.max(maxY, extent.maxY);
    }
    if (odom) {
      minX = Math.min(minX, odom.xM);
      maxX = Math.max(maxX, odom.xM);
      minY = Math.min(minY, odom.yM);
      maxY = Math.max(maxY, odom.yM);
    }

    const padding = MAP_PADDING_M; // metres of margin around extent
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
    ctx.strokeStyle = MAP_GRID_COLOR;
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
    ctx.fillStyle = MAP_POINT_COLOR;
    pts.forEach((p) => {
      const cx = toCanvasX(p.x);
      const cy = toCanvasY(p.y);
      ctx.fillRect(cx - SCAN_POINT_OFFSET_PX, cy - SCAN_POINT_OFFSET_PX, SCAN_POINT_SIZE_PX, SCAN_POINT_SIZE_PX);
    });

    // Robot position
    if (odom) {
      const rx = toCanvasX(odom.xM);
      const ry = toCanvasY(odom.yM);

      ctx.fillStyle = MAP_ROBOT_COLOR;
      ctx.beginPath();
      ctx.arc(rx, ry, MAP_ROBOT_RADIUS_PX, 0, Math.PI * 2);
      ctx.fill();

      ctx.strokeStyle = MAP_HEADING_COLOR;
      ctx.lineWidth = MAP_HEADING_STROKE_WIDTH_PX;
      ctx.beginPath();
      ctx.moveTo(rx, ry);
      ctx.lineTo(
        rx + Math.cos(odom.thetaRad) * MAP_ARROW_LEN_PX,
        ry - Math.sin(odom.thetaRad) * MAP_ARROW_LEN_PX,
      );
      ctx.stroke();
    }
  }, [recomputeExtentIfDirty]);

  const scheduleMapDraw = useCallback(() => {
    if (drawPendingRef.current) {
      return;
    }
    drawPendingRef.current = true;
    rafRef.current = requestAnimationFrame(() => {
      drawPendingRef.current = false;
      rafRef.current = null;
      drawMap();
    });
  }, [drawMap]);

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
      const point = {
        x: xM + d * Math.cos(thetaRad + a),
        y: yM + d * Math.sin(thetaRad + a),
      };
      const removed = buf.push(point);
      markExtentDirtyIfNeeded(removed);
      includePointInExtent(point);
    }

    updatePointCountThrottled();

    drawScan(scan);
    scheduleMapDraw();
  }, [drawScan, includePointInExtent, markExtentDirtyIfNeeded, scheduleMapDraw, updatePointCountThrottled]);

  // ── Subscribe (no React re-renders) ────────────────────────────────────
  useEffect(() => {
    let cancelled = false;

    const initial = useDashboardStore.getState().lidarScan;
    if (
      !cancelled &&
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
          !cancelled &&
          scan &&
          scan.angleRad.length === scan.rangeM.length &&
          scan.angleRad.length === scan.intensity.length
        ) {
          accumulateAndDraw(scan);
        }
      },
    );

    return () => {
      cancelled = true;
      unsub();
    };
  }, [accumulateAndDraw]);

  useEffect(() => {
    return () => {
      abortRef.current?.abort();
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
      }
    };
  }, []);

  // ── Reset ───────────────────────────────────────────────────────────────
  const handleReset = useCallback(async () => {
    pointBuffer.current.clear();
    extentRef.current = createEmptyExtent();
    updatePointCountThrottled(true);
    drawMap(); // clears immediately

    abortRef.current?.abort();
    const controller = new AbortController();
    abortRef.current = controller;

    try {
      await fetch('/api/slam/reset', { method: 'POST', signal: controller.signal });
    } catch (err) {
      if (err instanceof DOMException && err.name === 'AbortError') {
        return;
      }
      // Non-critical - map buffer already cleared client-side.
      console.error('Failed to reset SLAM buffer', err);
    }
  }, [drawMap, updatePointCountThrottled]);

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
            role="img"
            aria-label="scan preview canvas"
          />
        </div>
        <div style={SUB_PANEL}>
          <div style={SUB_HEADER}>Accumulated Map</div>
          <canvas
            ref={mapCanvasRef}
            width={MAP_SIZE_PX}
            height={MAP_SIZE_PX}
            style={CANVAS_STYLE}
            role="img"
            aria-label="accumulated map canvas"
          />
        </div>
      </div>
      <div style={FOOTER_STYLE}>
        <button type="button" style={RESET_BTN_STYLE} onClick={handleReset}>
          Reset Map
        </button>
        <span>
          Points: {pointCount.toLocaleString()} / {MAP_POINT_BUFFER_MAX.toLocaleString()} pts
        </span>
      </div>
    </div>
  );
}
