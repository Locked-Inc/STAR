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
});
