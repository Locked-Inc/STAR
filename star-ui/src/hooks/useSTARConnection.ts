import { useRef, useEffect } from 'react';
import { STAREnvelope } from '../proto/star/v1/ui';
import type { ControllerState } from '../proto/star/v1/controller';
import { useDashboardStore } from '../store/dashboardStore';
import { recordPacket } from '../services/GatewayService';

const MS_TO_US = 1000;
const BASE_DELAY_MS = 500;
const MAX_DELAY_MS = 30_000;
const JITTER_FACTOR = 0.3;

function getReconnectDelay(attempt: number): number {
  const exp = Math.min(BASE_DELAY_MS * 2 ** attempt, MAX_DELAY_MS);
  const jitter = exp * JITTER_FACTOR * (Math.random() * 2 - 1);
  return exp + jitter;
}

export function useSTARConnection(url: string): {
  sendControllerState: (state: ControllerState) => void;
  sendEStop: (reason: string) => void;
  sendEStopRelease: () => void;
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

  function sendRaw(env: STAREnvelope): void {
    const ws = wsRef.current;
    if (ws?.readyState !== WebSocket.OPEN) return;
    try {
      const bytes = STAREnvelope.toBinary(env);
      ws.send(bytes);
      recordPacket(env, 'tx', bytes.byteLength);
    } catch (e) {
      console.error('sendRaw: failed to encode/send envelope', env.payload.oneofKind, e);
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
          estop: { active: true, reason, timestampUs: String(Date.now() * MS_TO_US) },
        },
      });
      sendRaw(env);
    } else {
      // REST fallback -- WebSocket is not open
      fetch(`/api/estop?reason=${encodeURIComponent(reason)}`, { method: 'POST' }).catch(
        (err: unknown) => console.error('E-stop fallback failed', err)
      );
    }
  }

  function sendEStopRelease(): void {
    const env = STAREnvelope.create({
      payload: {
        oneofKind: 'estop',
        estop: { active: false, reason: 'User released E-Stop', timestampUs: String(Date.now() * MS_TO_US) },
      },
    });
    sendRaw(env);
  }

  return { sendControllerState, sendEStop, sendEStopRelease };
}
