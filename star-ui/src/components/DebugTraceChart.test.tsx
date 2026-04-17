import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';
import { DebugTraceChart } from './DebugTraceChart';

describe('DebugTraceChart', () => {
  it('renders a trace title and current value', () => {
    render(
      <DebugTraceChart
        accent="accent"
        formatValue={(value) => `${value.toFixed(1)} m/s`}
        samples={[0.2, 0.4, 0.8]}
        title="Velocity"
      />,
    );

    expect(screen.getByText('Velocity')).toBeInTheDocument();
    expect(screen.getByText('0.8 m/s')).toBeInTheDocument();
    expect(screen.getByText('3 pts')).toBeInTheDocument();
  });

  it('handles empty samples safely and shows zero point count', () => {
    expect(() =>
      render(
        <DebugTraceChart
          accent="accent"
          formatValue={(value) => `${value.toFixed(1)} m/s`}
          samples={[]}
          title="Velocity"
        />,
      ),
    ).not.toThrow();

    expect(screen.getByText('Velocity')).toBeInTheDocument();
    expect(screen.getByText('0 pts')).toBeInTheDocument();
  });

  it('handles a single sample safely and shows formatted current value', () => {
    expect(() =>
      render(
        <DebugTraceChart
          accent="accent"
          formatValue={(value) => `${value.toFixed(1)} m/s`}
          samples={[5]}
          title="Velocity"
        />,
      ),
    ).not.toThrow();

    expect(screen.getByText('Velocity')).toBeInTheDocument();
    expect(screen.getByText('1 pts')).toBeInTheDocument();
    expect(screen.getByText('5.0 m/s')).toBeInTheDocument();
  });

  it('ignores NaN sentinel samples when rendering current value and point count', () => {
    render(
      <DebugTraceChart
        accent="accent"
        formatValue={(value) => `${value.toFixed(1)} m/s`}
        samples={[Number.NaN, Number.NaN, 1.25]}
        title="Velocity"
      />,
    );

    expect(screen.getByText('Velocity')).toBeInTheDocument();
    expect(screen.getByText('1.3 m/s')).toBeInTheDocument();
    expect(screen.getByText('1 pts')).toBeInTheDocument();
  });
});
