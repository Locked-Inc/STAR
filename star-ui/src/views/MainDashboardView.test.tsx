import { render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useDashboardStore } from '../store/dashboardStore';
import { MainDashboardView } from './MainDashboardView';

vi.mock('../hooks/useControllerBridge', () => ({
  useControllerBridge: () => ({
    connected: false,
    linearVel: 0,
    angularVel: 0,
  }),
}));

describe('MainDashboardView', () => {
  beforeEach(() => {
    useDashboardStore.setState({
      alerts: [],
      connectionState: 'connected',
      dataIsStale: false,
      eStopActive: false,
      lidarScan: null,
      motors: [],
      odometry: null,
      seqGapDetected: false,
      systemStatus: null,
      telemetry: null,
    });
  });

  it('renders the main dashboard shell', () => {
    render(
      <MainDashboardView
        navigate={vi.fn()}
        sendControllerState={vi.fn()}
        sendEStop={vi.fn()}
        sendEStopRelease={vi.fn().mockResolvedValue(true)}
      />,
    );

    expect(screen.getByText('Project Star')).toBeInTheDocument();
    expect(screen.getByText('SLAM Map')).toBeInTheDocument();
    expect(screen.getByText('System Telemetry')).toBeInTheDocument();
  });
});
