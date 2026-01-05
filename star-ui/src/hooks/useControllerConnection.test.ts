import { renderHook } from '@testing-library/react';
import { useControllerConnection } from './useControllerConnection';
import { ControllerService } from '../services/ControllerService';
import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('../services/ControllerService', () => {
  const mockService = {
    connect: vi.fn(),
    disconnect: vi.fn(),
    sendState: vi.fn(),
  };
  return {
    ControllerService: vi.fn().mockImplementation(function(_url, onStatusChange) {
      mockService.connect.mockImplementation(() => onStatusChange(true));
      return mockService;
    }),
  };
});

describe('useControllerConnection', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should send stop command when gamepad disconnects', () => {
    const gamepadState = { connected: true, linearVel: 1.0, angularVel: 0.0 };
    const { rerender } = renderHook(({ state }) => useControllerConnection(state), {
      initialProps: { state: gamepadState },
    });

    rerender({ state: { connected: false, linearVel: 0, angularVel: 0 } });

    // Cast to unknown first to avoid eslint errors
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const mockService = (ControllerService as unknown as { mock: { results: { value: { sendState: any } }[] } });
    const serviceInstance = mockService.mock.results[0].value;

    expect(serviceInstance.sendState).toHaveBeenCalledWith(0, 0);
  });
});

