import { fireEvent, render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { usePacketFeed } from '../hooks/usePacketFeed';
import { useDashboardStore } from '../store/dashboardStore';
import { RosDebugView } from './RosDebugView';

vi.mock('../hooks/usePacketFeed', () => ({
  usePacketFeed: vi.fn(() => ({
    packets: [],
    sampledAtMs: 0,
  })),
}));

vi.mock('../hooks/useTraceHistory', () => ({
  useTraceHistory: () => ({
    cpu: [10, 20, 30],
    lidar: [0, 2, 4],
    temperature: [30, 31, 32],
    velocity: [0.1, 0.2, 0.3],
  }),
}));

const mockedUsePacketFeed = vi.mocked(usePacketFeed);

describe('RosDebugView', () => {
  beforeEach(() => {
    useDashboardStore.setState({
      ...useDashboardStore.getInitialState(),
      alerts: [],
      connectionState: 'connected',
      lidarScan: null,
      odometry: null,
      systemStatus: null,
      telemetry: null,
    }, true);
    mockedUsePacketFeed.mockReturnValue({
      packets: [],
      sampledAtMs: 0,
    });
  });

  it('renders the debug console shell', () => {
    render(<RosDebugView navigate={vi.fn()} />);

    expect(screen.getByText('ROS2 Debug Console')).toBeInTheDocument();
    expect(screen.getByText('Telemetry Traces')).toBeInTheDocument();
    expect(screen.getByText('System Logs')).toBeInTheDocument();
  });

  it('navigates back to control when the back control is clicked', () => {
    const navigate = vi.fn();
    render(<RosDebugView navigate={navigate} />);

    fireEvent.click(screen.getByRole('link', { name: /back to control/i }));

    expect(navigate).toHaveBeenCalledWith('/');
  });

  it('renders packet-backed log content when packet feed is non-empty', () => {
    mockedUsePacketFeed.mockReturnValue({
      packets: [
        {
          tsMs: 1_700_000_000_000,
          seq: 7,
          type: 'controller',
          direction: 'rx',
          sizeBytes: 64,
          preview: 'cmd_vel linear=0.35 angular=0.10',
        },
      ],
      sampledAtMs: 1_700_000_000_500,
    });

    render(<RosDebugView navigate={vi.fn()} />);

    expect(screen.getAllByText('cmd_vel linear=0.35 angular=0.10').length).toBeGreaterThan(0);
  });
});
