import { render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useDashboardStore } from '../store/dashboardStore';
import { RosDebugView } from './RosDebugView';

vi.mock('../hooks/usePacketFeed', () => ({
  usePacketFeed: () => ({
    packets: [],
    sampledAtMs: 0,
  }),
}));

vi.mock('../hooks/useTraceHistory', () => ({
  useTraceHistory: () => ({
    cpu: [10, 20, 30],
    lidar: [0, 2, 4],
    temperature: [30, 31, 32],
    velocity: [0.1, 0.2, 0.3],
  }),
}));

describe('RosDebugView', () => {
  beforeEach(() => {
    useDashboardStore.setState({
      alerts: [],
      connectionState: 'connected',
      lidarScan: null,
      odometry: null,
      systemStatus: null,
      telemetry: null,
    });
  });

  it('renders the debug console shell', () => {
    render(<RosDebugView navigate={vi.fn()} />);

    expect(screen.getByText('ROS2 Debug Console')).toBeInTheDocument();
    expect(screen.getByText('Telemetry Traces')).toBeInTheDocument();
    expect(screen.getByText('System Logs')).toBeInTheDocument();
  });
});
