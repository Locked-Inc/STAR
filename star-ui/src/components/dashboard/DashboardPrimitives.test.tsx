import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import { Chip, ControlButton, Glyph, HeaderLink, HealthRow, MetricTile } from './DashboardPrimitives';

describe('DashboardPrimitives', () => {
  it('renders shared dashboard UI primitives', () => {
    const onNavigate = vi.fn();
    const onClick = vi.fn();

    render(
      <div>
        <Glyph accent="good" label="PS" />
        <Chip tone="warn">Demo Mode</Chip>
        <MetricTile label="WiFi" tone="good" value="-62 dBm" detail="connected" />
        <HealthRow label="ROS2 Stack" tone="good" value="OK" />
        <HeaderLink href="/ros" onNavigate={onNavigate} label="Debug" accent="accent" glyph="RX" />
        <ControlButton label="Start" tone="good" onClick={onClick} />
      </div>,
    );

    fireEvent.click(screen.getByRole('link', { name: /debug/i }));
    fireEvent.click(screen.getByRole('button', { name: 'Start' }));

    expect(screen.getByText('Demo Mode')).toBeInTheDocument();
    expect(screen.getByText('-62 dBm')).toBeInTheDocument();
    expect(screen.getByText('ROS2 Stack')).toBeInTheDocument();
    expect(onNavigate).toHaveBeenCalledWith('/ros');
    expect(onClick).toHaveBeenCalled();
  });
});
