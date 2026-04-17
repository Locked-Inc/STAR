import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import { StarMapCanvas } from './StarMapCanvas';
import type { LidarScan, OdometryData } from '../proto/star/v1/ui';

describe('StarMapCanvas', () => {
  it('renders the empty-state overlay when odometry is unavailable', () => {
    const { container } = render(<StarMapCanvas lidarScan={null} odometry={null} />);

    expect(screen.getByText('Awaiting live odometry feed...')).toBeInTheDocument();
    expect(container.querySelector('canvas')).toBeInTheDocument();
  });
  
  it('hides the overlay and draws when odometry and lidar are provided', () => {
    const odometry: OdometryData = {
      xM: 1.25,
      yM: -0.75,
      thetaRad: Math.PI / 6,
      linearVelocityMps: 0.35,
      angularVelocityRadPerS: 0.12,
      timestampUs: '1713351234000000',
    };

    const lidarScan: LidarScan = {
      angleRad: [0, Math.PI / 8, Math.PI / 4, Math.PI / 2],
      rangeM: [1.2, 2.4, 3.1, 0.8],
      intensity: [12, 24, 36, 48],
      timestampUs: '1713351234001000',
    };

    expect(() =>
      render(<StarMapCanvas lidarScan={lidarScan} odometry={odometry} />),
    ).not.toThrow();
    expect(screen.queryByText('Awaiting live odometry feed...')).not.toBeInTheDocument();
  });
});
