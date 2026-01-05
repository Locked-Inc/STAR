import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { ControllerService } from './ControllerService';
import { ControllerState } from '../proto/star/v1/controller';

describe('ControllerService', () => {
  let service: ControllerService;
  let mockServer: any;

  beforeEach(() => {
    // Mock WebSocket
    class MockWebSocket {
      static OPEN = 1;
      url: string;
      send = vi.fn();
      close = vi.fn();
      readyState = 1; // OPEN
      binaryType = '';
      onopen = () => {};
      onclose = () => {};
      onerror = () => {};
      constructor(url: string) {
        this.url = url;
      }
    }
    vi.stubGlobal('WebSocket', MockWebSocket);
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('should send binary protobuf messages', () => {
    service = new ControllerService('ws://localhost:8080/ws/controller');
    service.connect();

    // @ts-ignore
    const socket = service['socket'];
    
    service.sendState(1.0, -0.5);

    expect(socket.send).toHaveBeenCalled();
    const sentBytes = socket.send.mock.calls[0][0];
    expect(sentBytes).toBeInstanceOf(Uint8Array);

    const decoded = ControllerState.fromBinary(sentBytes);
    expect(decoded.linearVel).toBe(1.0);
    expect(decoded.angularVel).toBe(-0.5);
  });
});
