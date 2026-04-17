import { act, renderHook } from '@testing-library/react';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { coverageTickIntervalMs } from '../lib/dashboard';
import { useDashboardStore } from '../store/dashboardStore';
import { useOperatorControls } from './useOperatorControls';

describe('useOperatorControls', () => {
  beforeEach(() => {
    vi.useFakeTimers();
    useDashboardStore.setState({ alerts: [], eStopActive: false });
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
    useDashboardStore.setState({ alerts: [], eStopActive: false });
  });

  it('handles E-stop release happy path without fallback alert', async () => {
    useDashboardStore.setState({ eStopActive: true, alerts: [] });
    const sendEStop = vi.fn();
    const sendEStopRelease = vi.fn().mockResolvedValue(true);

    const { result } = renderHook(() =>
      useOperatorControls({
        demoMode: true,
        eStopActive: true,
        reportedMode: null,
        sendEStop,
        sendEStopRelease,
      })
    );

    await act(async () => {
      await result.current.handleEStopToggle();
    });

    expect(sendEStopRelease).toHaveBeenCalledTimes(1);
    expect(sendEStop).not.toHaveBeenCalled();
    expect(useDashboardStore.getState().eStopActive).toBe(false);
    expect(useDashboardStore.getState().alerts).toEqual([]);
  });

  it('adds fallback alert and restores local run state when backend release confirmation is missing', async () => {
    useDashboardStore.setState({ eStopActive: false, alerts: [] });
    const sendEStop = vi.fn();
    const sendEStopRelease = vi.fn().mockResolvedValue(false);

    const { result, rerender } = renderHook(
      ({ eStopActive }) =>
        useOperatorControls({
          demoMode: true,
          eStopActive,
          reportedMode: null,
          sendEStop,
          sendEStopRelease,
        }),
      { initialProps: { eStopActive: false } }
    );

    act(() => {
      result.current.handleAutonomyStart();
    });

    act(() => {
      vi.advanceTimersByTime(coverageTickIntervalMs * 2);
    });

    const coverageBeforeEStop = result.current.coveragePercent;
    expect(coverageBeforeEStop).toBeGreaterThan(0);
    expect(result.current.autonomyRunning).toBe(true);

    await act(async () => {
      await result.current.handleEStopToggle();
    });

    expect(sendEStop).toHaveBeenCalledWith('user_ui_button');
    expect(useDashboardStore.getState().eStopActive).toBe(true);

    rerender({ eStopActive: true });

    await act(async () => {
      await result.current.handleEStopToggle();
    });

    expect(sendEStopRelease).toHaveBeenCalledTimes(1);
    expect(useDashboardStore.getState().eStopActive).toBe(false);
    expect(result.current.coveragePercent).toBeCloseTo(coverageBeforeEStop);
    expect(result.current.autonomyRunning).toBe(false);

    rerender({ eStopActive: false });

    expect(result.current.autonomyRunning).toBe(true);

    const fallbackAlert = useDashboardStore
      .getState()
      .alerts.find((alert) => alert.code === 'ESTOP_RESUME_LOCAL');
    expect(fallbackAlert).toBeDefined();
    expect(fallbackAlert?.message).toContain('Resume applied locally');
  });

  it('skips post-await store writes when unmounted before E-stop release resolves', async () => {
    useDashboardStore.setState({ eStopActive: true, alerts: [] });
    const sendEStop = vi.fn();
    let resolveRelease: ((released: boolean) => void) | undefined;
    const sendEStopRelease = vi.fn(
      () =>
        new Promise<boolean>((resolve) => {
          resolveRelease = resolve;
        })
    );

    const { result, unmount } = renderHook(() =>
      useOperatorControls({
        demoMode: true,
        eStopActive: true,
        reportedMode: null,
        sendEStop,
        sendEStopRelease,
      })
    );

    const togglePromise = act(async () => {
      await result.current.handleEStopToggle();
    });

    unmount();

    act(() => {
      resolveRelease?.(true);
    });

    await togglePromise;

    expect(sendEStopRelease).toHaveBeenCalledTimes(1);
    expect(sendEStop).not.toHaveBeenCalled();
    expect(useDashboardStore.getState().eStopActive).toBe(true);
    expect(useDashboardStore.getState().alerts).toEqual([]);
  });

  it('blocks mode changes when modeChangeDisabled is true due to reported mode', () => {
    const sendEStop = vi.fn();
    const sendEStopRelease = vi.fn().mockResolvedValue(true);

    const { result, rerender } = renderHook(
      ({ reportedMode }) =>
        useOperatorControls({
          demoMode: true,
          eStopActive: false,
          reportedMode,
          sendEStop,
          sendEStopRelease,
        }),
      { initialProps: { reportedMode: null as 'autonomous' | 'manual' | null } }
    );

    expect(result.current.modeChangeDisabled).toBe(false);
    expect(result.current.uiMode).toBe('autonomous');

    act(() => {
      result.current.handleModeToggle();
    });

    expect(result.current.uiMode).toBe('manual');

    rerender({ reportedMode: 'manual' });

    expect(result.current.modeChangeDisabled).toBe(true);

    act(() => {
      result.current.handleModeToggle();
    });

    expect(result.current.uiMode).toBe('manual');
  });

  it('drives demo-mode interval to completion and marks task completed at 100 coverage', () => {
    const sendEStop = vi.fn();
    const sendEStopRelease = vi.fn().mockResolvedValue(true);

    const { result } = renderHook(() =>
      useOperatorControls({
        demoMode: true,
        eStopActive: false,
        reportedMode: null,
        sendEStop,
        sendEStopRelease,
      })
    );

    act(() => {
      result.current.handleAutonomyStart();
    });

    act(() => {
      vi.advanceTimersByTime(coverageTickIntervalMs * 80);
    });

    expect(result.current.coveragePercent).toBe(100);
    expect(result.current.taskCompleted).toBe(true);
    expect(result.current.autonomyRunning).toBe(false);
  });

  it('emits unavailable-progress alert when autonomy start is requested outside demo mode', () => {
    const sendEStop = vi.fn();
    const sendEStopRelease = vi.fn().mockResolvedValue(true);

    const { result } = renderHook(() =>
      useOperatorControls({
        demoMode: false,
        eStopActive: false,
        reportedMode: null,
        sendEStop,
        sendEStopRelease,
      })
    );

    act(() => {
      result.current.handleAutonomyStart();
    });

    expect(result.current.coveragePercent).toBe(0);
    expect(result.current.taskCompleted).toBe(false);
    expect(result.current.autonomyRunning).toBe(false);

    const unavailableAlert = useDashboardStore
      .getState()
      .alerts.find((alert) => alert.code === 'MISSION_PROGRESS_UNAVAILABLE');
    expect(unavailableAlert).toBeDefined();
    expect(unavailableAlert?.message).toContain('Live mission progress is unavailable');
  });
});
