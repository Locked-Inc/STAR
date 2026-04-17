import { useEffect, useRef } from 'react';
import type { LidarScan, OdometryData } from '../proto/star/v1/ui';

interface StarMapCanvasProps {
  lidarScan: LidarScan | null;
  odometry: OdometryData | null;
}

interface WorldPoint {
  x: number;
  y: number;
}

const canvasWidth = 600;
const canvasHeight = 280;
const pixelsPerMeter = 42;
const maxPathPoints = 220;
const maxObstaclePoints = 5000;
const gridSpacingMeters = 0.5;
const lidarRangeLimitMeters = 12;

function project(point: WorldPoint, origin: WorldPoint): { x: number; y: number } {
  return {
    x: canvasWidth / 2 + (point.x - origin.x) * pixelsPerMeter,
    y: canvasHeight / 2 - (point.y - origin.y) * pixelsPerMeter,
  };
}

export function StarMapCanvas({ lidarScan, odometry }: StarMapCanvasProps) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const pathRef = useRef<WorldPoint[]>([]);
  const obstacleRef = useRef<WorldPoint[]>([]);

  useEffect(() => {
    if (!odometry) {
      return;
    }

    const latestPoint = { x: odometry.xM, y: odometry.yM };
    const currentPath = pathRef.current;
    const previousPoint = currentPath[currentPath.length - 1];

    if (!previousPoint || Math.hypot(previousPoint.x - latestPoint.x, previousPoint.y - latestPoint.y) > 0.05) {
      pathRef.current = [...currentPath, latestPoint].slice(-maxPathPoints);
    }
  }, [odometry]);

  useEffect(() => {
    if (!lidarScan || !odometry) {
      return;
    }

    const nextPoints: WorldPoint[] = [];
    for (let index = 0; index < lidarScan.angleRad.length; index += 1) {
      const range = lidarScan.rangeM[index];
      const angle = lidarScan.angleRad[index];
      if (!Number.isFinite(range) || !Number.isFinite(angle) || range <= 0 || range > lidarRangeLimitMeters) {
        continue;
      }

      const worldAngle = odometry.thetaRad + angle;
      nextPoints.push({
        x: odometry.xM + Math.cos(worldAngle) * range,
        y: odometry.yM + Math.sin(worldAngle) * range,
      });
    }

    obstacleRef.current = [...obstacleRef.current, ...nextPoints].slice(-maxObstaclePoints);
  }, [lidarScan, odometry]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) {
      return;
    }

    const context = canvas.getContext('2d');
    if (!context) {
      return;
    }

    const origin = odometry ? { x: odometry.xM, y: odometry.yM } : { x: 0, y: 0 };

    context.clearRect(0, 0, canvasWidth, canvasHeight);

    context.fillStyle = '#0d1520';
    context.fillRect(0, 0, canvasWidth, canvasHeight);

    context.save();
    context.strokeStyle = 'rgba(112, 145, 183, 0.18)';
    context.lineWidth = 1;
    for (let offset = -20; offset <= 20; offset += gridSpacingMeters) {
      const gridPx = gridSpacingMeters * pixelsPerMeter;
      const xMod = (((origin.x * pixelsPerMeter) % gridPx) + gridPx) % gridPx;
      const x = canvasWidth / 2 + offset * pixelsPerMeter - xMod;
      context.moveTo(x, 0);
      context.lineTo(x, canvasHeight);
      context.stroke();

      const yMod = (((origin.y * pixelsPerMeter) % gridPx) + gridPx) % gridPx;
      const y = canvasHeight / 2 + offset * pixelsPerMeter - yMod;
      context.beginPath();
      context.moveTo(0, y);
      context.lineTo(canvasWidth, y);
      context.stroke();
    }
    context.restore();

    if (pathRef.current.length > 1) {
      context.save();
      context.strokeStyle = '#facc15';
      context.lineWidth = 2.5;
      context.lineCap = 'round';
      context.shadowColor = 'rgba(250, 204, 21, 0.45)';
      context.shadowBlur = 12;
      context.beginPath();

      pathRef.current.forEach((point, index) => {
        const projected = project(point, origin);
        if (index === 0) {
          context.moveTo(projected.x, projected.y);
        } else {
          context.lineTo(projected.x, projected.y);
        }
      });

      context.stroke();
      context.restore();
    }

    context.save();
    context.fillStyle = 'rgba(0, 0, 0, 0.9)';
    obstacleRef.current.forEach((point) => {
      const projected = project(point, origin);
      if (projected.x < -8 || projected.x > canvasWidth + 8 || projected.y < -8 || projected.y > canvasHeight + 8) {
        return;
      }
      context.beginPath();
      context.arc(projected.x, projected.y, 1.25, 0, Math.PI * 2);
      context.fill();
    });
    context.restore();

    if (lidarScan && odometry) {
      context.save();
      context.lineWidth = 1;
      context.shadowColor = 'rgba(255, 91, 91, 0.22)';
      context.shadowBlur = 8;
      for (let index = 0; index < lidarScan.angleRad.length; index += 1) {
        const range = lidarScan.rangeM[index];
        const angle = lidarScan.angleRad[index];
        if (!Number.isFinite(range) || !Number.isFinite(angle) || range <= 0 || range > lidarRangeLimitMeters) {
          continue;
        }

        const beamEnd = {
          x: origin.x + Math.cos((odometry?.thetaRad ?? 0) + angle) * range,
          y: origin.y + Math.sin((odometry?.thetaRad ?? 0) + angle) * range,
        };
        const projected = project(beamEnd, origin);
        const gradient = context.createLinearGradient(canvasWidth / 2, canvasHeight / 2, projected.x, projected.y);
        gradient.addColorStop(0, 'rgba(255, 92, 92, 0.32)');
        gradient.addColorStop(1, 'rgba(255, 92, 92, 0.04)');
        context.strokeStyle = gradient;
        context.beginPath();
        context.moveTo(canvasWidth / 2, canvasHeight / 2);
        context.lineTo(projected.x, projected.y);
        context.stroke();
      }
      context.restore();
    }

    context.save();
    context.translate(canvasWidth / 2, canvasHeight / 2);
    context.rotate(odometry?.thetaRad ?? 0);

    context.fillStyle = '#111827';
    context.strokeStyle = '#31425f';
    context.lineWidth = 1.2;
    context.fillRect(-10, -8, 20, 16);
    context.strokeRect(-10, -8, 20, 16);

    context.strokeStyle = '#ef4444';
    context.lineWidth = 2.2;
    context.beginPath();
    context.moveTo(0, 0);
    context.lineTo(24, 0);
    context.stroke();

    context.fillStyle = '#ef4444';
    context.beginPath();
    context.moveTo(26, 0);
    context.lineTo(20, -4);
    context.lineTo(20, 4);
    context.closePath();
    context.fill();

    context.strokeStyle = '#22c55e';
    context.beginPath();
    context.moveTo(0, 0);
    context.lineTo(0, -22);
    context.stroke();

    context.fillStyle = '#22c55e';
    context.beginPath();
    context.moveTo(0, -24);
    context.lineTo(-4, -18);
    context.lineTo(4, -18);
    context.closePath();
    context.fill();

    context.fillStyle = '#38bdf8';
    context.beginPath();
    context.arc(0, 0, 2.2, 0, Math.PI * 2);
    context.fill();
    context.restore();
  }, [lidarScan, odometry]);

  return (
    <div className="map-surface">
      <canvas className="map-surface__canvas" height={canvasHeight} ref={canvasRef} width={canvasWidth} />
      {!odometry ? <div className="map-surface__overlay">Awaiting live odometry feed...</div> : null}
    </div>
  );
}
