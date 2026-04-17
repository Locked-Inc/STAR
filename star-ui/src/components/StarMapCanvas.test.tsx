import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import { StarMapCanvas } from './StarMapCanvas';

describe('StarMapCanvas', () => {
  it('renders the empty-state overlay when odometry is unavailable', () => {
    const { container } = render(<StarMapCanvas lidarScan={null} odometry={null} />);

    expect(screen.getByText('Awaiting live odometry feed...')).toBeInTheDocument();
    expect(container.querySelector('canvas')).toBeInTheDocument();
  });
  
  it('hides the overlay when odometry is provided', () => {
    const odometry = {
      linearVelocityMps: 0.1,
      angularVelocityRadps: 0,
      positionX: 0, positionY: 0, headingRad: 0,
      // ...fill in remaining required fields per OdometryData
    } as unknown as Parameters<typeof StarMapCanvas>[0]['odometry'];
    render(<StarMapCanvas lidarScan={null} odometry={odometry} />);
    expect(screen.queryByText('Awaiting live odometry feed...')).not.toBeInTheDocument();
  });
});
