import { useRef, useEffect } from 'react';
import { STAREnvelope } from '../proto/star/v1/ui';
import type { ControllerState } from '../proto/star/v1/controller';
import { useDashboardStore } from '../store/dashboardStore';
import { recordPacket } from '../services/GatewayService';

const msToUs = 1000;
const baseDelayMs = 500;
const maxDelayMs = 30_000;
const jitterFactor = 0.3;

function getReconnectDelay(attempt: number): number {
  const exp = Math.min(baseDelayMs * 2 ** attempt, maxDelayMs);
  const jitter = exp * jitterFactor * (Math.random() * 2 - 1);
  return exp + jitter;
}

export function useSTARConnection(url: string): {
  sendControllerState: (state: ControllerState) => void;
  sendEStop: (reason: string) => void;
  sendEStopRelease: () => Promise<boolean>;
} {
  const wsRef = useRef<WebSocket | null>(null);
  const attemptRef = useRef(0);
  const timeoutRef = useRef<ReturnType<typeof window.setTimeout> | null>(null);

  useEffect(() => {
    let cancelled = false;
    const store = useDashboardStore;

    function connect(): void {
      store.getState().setConnectionState('connecting');
      const ws = new WebSocket(url);

      // CRITICAL: Must be set before any messages arrive.
      // Browser default is 'blob' -- Uint8Array cannot be constructed from Blob.
      ws.binaryType = 'arraybuffer';

      ws.onopen = () => {
        attemptRef.current = 0;
        store.getState().setConnectionState('connected');
      };

      ws.onmessage = (event: MessageEvent) => {
        const raw = event.data as ArrayBuffer;
        const byteLength = raw.byteLength; // capture BEFORE decode
        let env: STAREnvelope;
        try {
          env = STAREnvelope.fromBinary(new Uint8Array(raw));
        } catch (e) {
          recordPacket(null, 'rx', byteLength, 'DECODE_ERROR');
          console.warn('STAREnvelope decode failed', e);
          return;
        }

        const state = store.getState();
        state.updateFromEnvelope(env);
        recordPacket(env, 'rx', byteLength);
      };

      ws.onclose = () => {
        if (cancelled) return; // StrictMode cleanup: skip reconnect
        store.getState().markStale();
        store.getState().setConnectionState('reconnecting');
        const delay = getReconnectDelay(attemptRef.current++);
        timeoutRef.current = window.setTimeout(() => { if (!cancelled) connect(); }, delay);
      };

      ws.onerror = () => {
        // onerror is always followed by onclose, so reconnect logic is there
      };

      wsRef.current = ws;
    }

    connect();

    return () => {
      cancelled = true;
      wsRef.current?.close();
      if (timeoutRef.current !== null) clearTimeout(timeoutRef.current);
    };
  }, [url]);

  function sendRaw(env: STAREnvelope): boolean {
    const ws = wsRef.current;
    if (ws?.readyState !== WebSocket.OPEN) return false;
    try {
      const bytes = STAREnvelope.toBinary(env);
      ws.send(bytes);
      recordPacket(env, 'tx', bytes.byteLength);
      return true;
    } catch (e) {
      console.error('sendRaw: failed to encode/send envelope', env.payload.oneofKind, e);
      return false;
    }
  }

  async function postEStopFallback(action: 'activate' | 'release', reason: string): Promise<boolean> {
    try {
      const params = new URLSearchParams({ action, reason });
      const response = await fetch(`/api/estop?${params.toString()}`, { method: 'POST' });
      return response.ok;
    } catch (err: unknown) {
      console.error('E-stop fallback failed', err);
      return false;
    }
  }

  function sendControllerState(state: ControllerState): void {
    const env = STAREnvelope.create({
      payload: { oneofKind: 'controller', controller: state },
    });
    sendRaw(env);
  }

  function sendEStop(reason: string): void {
    const ws = wsRef.current;
    if (ws?.readyState === WebSocket.OPEN) {
      const env = STAREnvelope.create({
        payload: {
          oneofKind: 'estop',
          estop: { active: true, reason, timestampUs: String(Date.now() * msToUs) },
        },
      });
      sendRaw(env);
    } else {
      // REST fallback -- WebSocket is not open
      void postEStopFallback('activate', reason);
    }
  }

  async function sendEStopRelease(): Promise<boolean> {
    const ws = wsRef.current;
    const env = STAREnvelope.create({
      payload: {
        oneofKind: 'estop',
        estop: { active: false, reason: 'User released E-Stop', timestampUs: String(Date.now() * msToUs) },
      },
    });

    if (ws?.readyState === WebSocket.OPEN) {
      return sendRaw(env);
    }

    return postEStopFallback('release', 'User released E-Stop');
  }

  return { sendControllerState, sendEStop, sendEStopRelease };
}
