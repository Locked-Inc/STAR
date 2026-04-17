/**
 * @file StarMapCanvas.tsx
 * @brief Canvas renderer for live SLAM map, robot pose, and LIDAR obstacles.
 * @copyright Copyright (c) Project Star UI Contributors.
 * @license Licensed under the repository license.
 */
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
const pathPointThresholdMeters = 0.05;
const obstacleCullMarginPx = 8;
const canvasCenterXPx = canvasWidth / 2;
const canvasCenterYPx = canvasHeight / 2;

const robotWidthMeters = 20 / pixelsPerMeter;
const robotHeightMeters = 16 / pixelsPerMeter;
const robotBodyWidthPx = robotWidthMeters * pixelsPerMeter;
const robotBodyHeightPx = robotHeightMeters * pixelsPerMeter;
const robotHalfWidthPx = robotBodyWidthPx / 2;
const robotHalfHeightPx = robotBodyHeightPx / 2;

const robotBodyLineWidthPx = 1.2;
const robotAxisLineWidthPx = 2.2;

const headingLengthMeters = 24 / pixelsPerMeter;
const headingLengthPx = headingLengthMeters * pixelsPerMeter;
const headingArrowTipOffsetMeters = 2 / pixelsPerMeter;
const headingArrowTipOffsetPx = headingArrowTipOffsetMeters * pixelsPerMeter;
const headingArrowHalfWidthMeters = 4 / pixelsPerMeter;
const headingArrowHalfWidthPx = headingArrowHalfWidthMeters * pixelsPerMeter;

const referenceTickLengthMeters = 22 / pixelsPerMeter;
const referenceTickLengthPx = referenceTickLengthMeters * pixelsPerMeter;
const referenceArrowTipOffsetMeters = 2 / pixelsPerMeter;
const referenceArrowTipOffsetPx = referenceArrowTipOffsetMeters * pixelsPerMeter;
const referenceArrowBaseOffsetMeters = 4 / pixelsPerMeter;
const referenceArrowBaseOffsetPx = referenceArrowBaseOffsetMeters * pixelsPerMeter;
const referenceArrowHalfWidthMeters = 4 / pixelsPerMeter;
const referenceArrowHalfWidthPx = referenceArrowHalfWidthMeters * pixelsPerMeter;

const markerRadiusMeters = 2.2 / pixelsPerMeter;
const markerRadiusPx = markerRadiusMeters * pixelsPerMeter;

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
      pathRef.current = [];
      return;
    }

    const latestPoint = { x: odometry.xM, y: odometry.yM };
    const currentPath = pathRef.current;
    const previousPoint = currentPath[currentPath.length - 1];

    if (!previousPoint || Math.hypot(previousPoint.x - latestPoint.x, previousPoint.y - latestPoint.y) > pathPointThresholdMeters) {
      pathRef.current = [...currentPath, latestPoint].slice(-maxPathPoints);
    }
  }, [odometry]);

  useEffect(() => {
    if (!lidarScan || !odometry) {
      obstacleRef.current = [];
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

    const halfWidthMeters = canvasWidth / 2 / pixelsPerMeter;
    const halfHeightMeters = canvasHeight / 2 / pixelsPerMeter;
    const gridHalfExtentMeters = Math.ceil(Math.max(halfWidthMeters, halfHeightMeters) + gridSpacingMeters);

    for (let offset = -gridHalfExtentMeters; offset <= gridHalfExtentMeters; offset += gridSpacingMeters) {
      const gridPx = gridSpacingMeters * pixelsPerMeter;
      const xMod = (((origin.x * pixelsPerMeter) % gridPx) + gridPx) % gridPx;
      const x = canvasWidth / 2 + offset * pixelsPerMeter - xMod;
      context.beginPath();
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
      if (projected.x < -obstacleCullMarginPx || projected.x > canvasWidth + obstacleCullMarginPx || projected.y < -obstacleCullMarginPx || projected.y > canvasHeight + obstacleCullMarginPx) {
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

        const worldAngle = odometry.thetaRad + angle;
        const beamEnd = {
          x: origin.x + Math.cos(worldAngle) * range,
          y: origin.y + Math.sin(worldAngle) * range,
        };
        const projected = project(beamEnd, origin);
        const gradient = context.createLinearGradient(canvasCenterXPx, canvasCenterYPx, projected.x, projected.y);
        gradient.addColorStop(0, 'rgba(255, 92, 92, 0.32)');
        gradient.addColorStop(1, 'rgba(255, 92, 92, 0.04)');
        context.strokeStyle = gradient;
        context.beginPath();
        context.moveTo(canvasCenterXPx, canvasCenterYPx);
        context.lineTo(projected.x, projected.y);
        context.stroke();
      }
      context.restore();
    }

    if (odometry) {
      context.save();
      context.translate(canvasCenterXPx, canvasCenterYPx);
      context.rotate(odometry.thetaRad);

      context.fillStyle = '#111827';
      context.strokeStyle = '#31425f';
      context.lineWidth = robotBodyLineWidthPx;
      context.fillRect(-robotHalfWidthPx, -robotHalfHeightPx, robotBodyWidthPx, robotBodyHeightPx);
      context.strokeRect(-robotHalfWidthPx, -robotHalfHeightPx, robotBodyWidthPx, robotBodyHeightPx);

      context.strokeStyle = '#ef4444';
      context.lineWidth = robotAxisLineWidthPx;
      context.beginPath();
      context.moveTo(0, 0);
      context.lineTo(headingLengthPx, 0);
      context.stroke();

      context.fillStyle = '#ef4444';
      context.beginPath();
      context.moveTo(headingLengthPx + headingArrowTipOffsetPx, 0);
      context.lineTo(headingLengthPx - headingArrowHalfWidthPx, -headingArrowHalfWidthPx);
      context.lineTo(headingLengthPx - headingArrowHalfWidthPx, headingArrowHalfWidthPx);
      context.closePath();
      context.fill();

      context.strokeStyle = '#22c55e';
      context.beginPath();
      context.moveTo(0, 0);
      context.lineTo(0, -referenceTickLengthPx);
      context.stroke();

      context.fillStyle = '#22c55e';
      context.beginPath();
      context.moveTo(0, -(referenceTickLengthPx + referenceArrowTipOffsetPx));
      context.lineTo(-referenceArrowHalfWidthPx, -(referenceTickLengthPx - referenceArrowBaseOffsetPx));
      context.lineTo(referenceArrowHalfWidthPx, -(referenceTickLengthPx - referenceArrowBaseOffsetPx));
      context.closePath();
      context.fill();

      context.fillStyle = '#38bdf8';
      context.beginPath();
      context.arc(0, 0, markerRadiusPx, 0, Math.PI * 2);
      context.fill();
      context.restore();
    }
  }, [lidarScan, odometry]);

  return (
    <div className="map-surface">
      <canvas className="map-surface__canvas" height={canvasHeight} ref={canvasRef} width={canvasWidth} />
      {!odometry ? <div className="map-surface__overlay">Awaiting live odometry feed...</div> : null}
    </div>
  );
}
