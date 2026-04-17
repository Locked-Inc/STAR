import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import { ImuOrientationGraphic } from './ImuOrientationGraphic';

describe('ImuOrientationGraphic', () => {
  it('renders the IMU visual label', () => {
    render(<ImuOrientationGraphic pitchDeg={4} rollDeg={-6} />);

    expect(screen.getByText('Visual Orientation')).toBeInTheDocument();
  });
});
