import { act, fireEvent, render, screen } from '@testing-library/react';
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
      connected: true,
      linearVel: 0.42,
      angularVel: 0.18,
    };
    mockedUseControllerBridge.mockReturnValue(gamepadState);

    useDashboardStore.setState({
      ...useDashboardStore.getInitialState(),
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
    }, true);
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

  it('exercises injected callbacks through UI interactions', async () => {
    const navigate = vi.fn();
    const sendControllerState = vi.fn();
    const sendEStop = vi.fn();
    const sendEStopRelease = vi.fn().mockResolvedValue(true);

    render(
      <MainDashboardView
        navigate={navigate}
        sendControllerState={sendControllerState}
        sendEStop={sendEStop}
        sendEStopRelease={sendEStopRelease}
      />,
    );

    expect(mockedUseControllerBridge).toHaveBeenCalledWith(sendControllerState, false);

    await act(async () => {
      fireEvent.click(screen.getByRole('link', { name: /debug/i }));
    });
    expect(navigate).toHaveBeenCalledWith('/ros');

    await act(async () => {
      fireEvent.click(screen.getByRole('button', { name: 'EMERGENCY STOP' }));
    });
    expect(sendEStop).toHaveBeenCalledWith('user_ui_button');

    await act(async () => {
      fireEvent.click(screen.getByRole('button', { name: 'RESUME' }));
    });
    expect(sendEStopRelease).toHaveBeenCalledTimes(1);
  });

  it('shows connected controller state and non-zero manual velocities', () => {
    render(
      <MainDashboardView
        navigate={vi.fn()}
        sendControllerState={vi.fn()}
        sendEStop={vi.fn()}
        sendEStopRelease={vi.fn().mockResolvedValue(true)}
      />,
    );

    act(() => {
      fireEvent.click(screen.getByRole('button', { name: 'Autonomous Mode' }));
    });

    expect(screen.getByText('Gamepad Connected')).toBeInTheDocument();
    expect(screen.getByText('0.42')).toBeInTheDocument();
    expect(screen.getByText('0.18')).toBeInTheDocument();
  });
});
