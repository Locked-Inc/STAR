import { renderHook, act } from '@testing-library/react';
import { useGamepad } from './useGamepad';
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

describe('useGamepad', () => {
  beforeEach(() => {
    vi.useFakeTimers();
    // Mock requestAnimationFrame
    vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => setTimeout(cb, 16));
    vi.stubGlobal('cancelAnimationFrame', (id: number) => clearTimeout(id));
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('should initialize with disconnected state', () => {
    vi.stubGlobal('navigator', {
      getGamepads: vi.fn().mockReturnValue([null]),
    });

    const { result } = renderHook(() => useGamepad());
    expect(result.current.connected).toBe(false);
    expect(result.current.linearVel).toBe(0);
    expect(result.current.angularVel).toBe(0);
  });

  it('should detect a connected gamepad and update values', async () => {
    const mockGamepad = {
      axes: [0.5, -0.8], // X: 0.5, Y: -0.8
      buttons: [],
      connected: true,
    };

    const getGamepads = vi.fn().mockReturnValue([mockGamepad]);
    vi.stubGlobal('navigator', { getGamepads });

    const { result } = renderHook(() => useGamepad());

    // Advance time to trigger polling
    await act(async () => {
      vi.advanceTimersByTime(20);
    });

    expect(result.current.connected).toBe(true);
    // Linear is -axes[1] -> -(-0.8) = 0.8
    expect(result.current.linearVel).toBeCloseTo(0.8);
    // Angular is axes[0] -> 0.5
    expect(result.current.angularVel).toBeCloseTo(0.5);
  });

  it('should apply deadzone', async () => {
    const mockGamepad = {
      axes: [0.05, -0.05], // Values below 0.1 deadzone
      buttons: [],
      connected: true,
    };

    vi.stubGlobal('navigator', {
      getGamepads: vi.fn().mockReturnValue([mockGamepad]),
    });

    const { result } = renderHook(() => useGamepad());

    await act(async () => {
      vi.advanceTimersByTime(20);
    });

    expect(result.current.linearVel).toBe(0);
    expect(result.current.angularVel).toBe(0);
  });
});
