import { ControllerState } from '../proto/star/v1/controller';

export class ControllerService {
  private socket: WebSocket | null = null;
  private url: string;
  private onStatusChange?: (connected: boolean) => void;
  private intentionalDisconnect = false;

  constructor(url: string, onStatusChange?: (connected: boolean) => void) {
    this.url = url;
    this.onStatusChange = onStatusChange;
  }

  connect() {
    if (this.socket) return;
    this.intentionalDisconnect = false;

    this.socket = new WebSocket(this.url);
    this.socket.binaryType = 'arraybuffer';

    this.socket.onopen = () => {
      console.log('WebSocket Connected');
      this.onStatusChange?.(true);
    };

    this.socket.onclose = () => {
      console.log('WebSocket Disconnected');
      this.onStatusChange?.(false);
      this.socket = null;
      
      if (!this.intentionalDisconnect) {
        console.log('Attempting to reconnect in 1s...');
        setTimeout(() => this.connect(), 1000);
      }
    };

    this.socket.onerror = (error) => {
      console.error('WebSocket Error:', error);
    };
  }

  sendState(linearVel: number, angularVel: number) {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) return;

    const state: ControllerState = {
      linearVel,
      angularVel,
      timestamp: Date.now().toString(), // long_type_string option was used
    };

    const bytes = ControllerState.toBinary(state);
    this.socket.send(bytes);
  }

  disconnect() {
    this.intentionalDisconnect = true;
    if (this.socket) {
      // Before closing, send a final stop command for safety
      if (this.socket.readyState === WebSocket.OPEN) {
        this.sendState(0, 0);
      }
      this.socket.close();
      this.socket = null;
    }
  }
}
