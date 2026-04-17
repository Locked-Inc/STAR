import { render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useControllerBridge } from '../hooks/useControllerBridge';
import { useDashboardStore } from '../store/dashboardStore';
import { MainDashboardView } from './MainDashboardView';

vi.mock('../hooks/useControllerBridge', () => ({
  useControllerBridge: vi.fn(),
}));

const mockedUseControllerBridge = vi.mocked(useControllerBridge);

describe('MainDashboardView', () => {
  beforeEach(() => {
    const gamepadState: ReturnType<typeof useControllerBridge> = {
      connected: false,
      linearVel: 0,
      angularVel: 0,
    };
    mockedUseControllerBridge.mockReturnValue(gamepadState);

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
