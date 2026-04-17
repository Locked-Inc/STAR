import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import { ImuOrientationGraphic } from './ImuOrientationGraphic';

describe('ImuOrientationGraphic', () => {
  it('renders the IMU visual label', () => {
    render(<ImuOrientationGraphic pitchDeg={4} rollDeg={-6} />);
    expect(screen.getByText('Visual Orientation')).toBeInTheDocument();
  });
  it('clamps extreme roll input to the visual limit', () => {
    const { container } = render(<ImuOrientationGraphic pitchDeg={0} rollDeg={9999} />);
    const group = container.querySelector('g[transform]');
    expect(group?.getAttribute('transform')).toMatch(/rotate\(22\)/);
  });
  it('reflects pitch in the deck translate', () => {
    const { container } = render(<ImuOrientationGraphic pitchDeg={10} rollDeg={0} />);
    const group = container.querySelector('g[transform]');
    expect(group?.getAttribute('transform')).toMatch(/translate\(50 54\.2\)/);
  });

  it('sanitizes NaN and Infinity inputs to stable transforms', () => {
    const { container } = render(<ImuOrientationGraphic pitchDeg={Number.NaN} rollDeg={Number.POSITIVE_INFINITY} />);
    const group = container.querySelector('g[transform]');
    const arrow = container.querySelector('path[fill="#ef4444"]');

    expect(group?.getAttribute('transform')).toBe('translate(50 50) rotate(0)');
    expect(group?.getAttribute('transform')).not.toMatch(/NaN|Infinity/);
    expect(arrow?.getAttribute('d')).not.toMatch(/NaN|Infinity/);
  });
});
