import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import { StarMapCanvas } from './StarMapCanvas';

describe('StarMapCanvas', () => {
  it('renders the empty-state overlay when odometry is unavailable', () => {
    const { container } = render(<StarMapCanvas lidarScan={null} odometry={null} />);

    expect(screen.getByText('Awaiting live odometry feed...')).toBeInTheDocument();
    expect(container.querySelector('canvas')).toBeInTheDocument();
  });
});
