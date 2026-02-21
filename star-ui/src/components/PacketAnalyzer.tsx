import { useEffect, useState } from 'react';
import { getRecentPackets } from '../services/GatewayService';
import type { PacketRecord } from '../services/GatewayService';

const MS_PER_SECOND = 1000;
const POLL_INTERVAL_MS = 200;
const MAX_PACKETS = 30;

const DIR_COLORS = { rx: '#22c55e', tx: '#3b82f6' } as const;
const TYPE_COLORS: Record<string, string> = {
  telemetry: '#a78bfa',
  motors: '#f59e0b',
  battery: '#34d399',
  odometry: '#60a5fa',
  lidar: '#f472b6',
  alert: '#ef4444',
  system: '#94a3b8',
  controller: '#818cf8',
  estop: '#dc2626',
  DECODE_ERROR: '#ef4444',
};

function ageStr(tsMs: number): string {
  const ms = Date.now() - tsMs;
  if (ms < MS_PER_SECOND) return `${ms}ms`;
  return `${(ms / MS_PER_SECOND).toFixed(1)}s`;
}

function PacketRow({ packet }: { packet: PacketRecord }) {
  const typeColor = TYPE_COLORS[packet.type] ?? '#9ca3af';
  const dirColor = DIR_COLORS[packet.direction];

  return (
    <tr style={{ borderBottom: '1px solid #1e2335' }}>
      <td style={{ padding: '2px 6px', fontFamily: 'monospace', color: '#9ca3af' }}>
        {packet.seq || '--'}
      </td>
      <td style={{ padding: '2px 6px', fontFamily: 'monospace', color: typeColor }}>
        {packet.type}
      </td>
      <td style={{ padding: '2px 6px', fontFamily: 'monospace', color: dirColor, fontWeight: 'bold' }}>
        {packet.direction.toUpperCase()}
      </td>
      <td style={{ padding: '2px 6px', fontFamily: 'monospace', color: '#9ca3af' }}>
        {packet.sizeBytes}B
      </td>
      <td style={{ padding: '2px 6px', fontFamily: 'monospace', color: '#6b7280' }}>
        {ageStr(packet.tsMs)}
      </td>
      <td
        style={{
          padding: '2px 6px',
          fontFamily: 'monospace',
          color: '#e2e8f0',
          maxWidth: '220px',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          whiteSpace: 'nowrap',
        }}
      >
        {packet.preview}
      </td>
    </tr>
  );
}

export function PacketAnalyzer() {
  const [packets, setPackets] = useState<PacketRecord[]>([]);

  useEffect(() => {
    const id = setInterval(() => {
      setPackets(getRecentPackets().slice(0, MAX_PACKETS));
    }, POLL_INTERVAL_MS);

    return () => clearInterval(id);
  }, []);

  return (
    <div
      style={{
        background: '#1a1d27',
        border: '1px solid #2a2e42',
        borderRadius: '6px',
        overflow: 'hidden',
      }}
    >
      <div
        style={{
          padding: '6px 10px',
          borderBottom: '1px solid #2a2e42',
          fontSize: '11px',
          fontWeight: 'bold',
          color: '#9ca3af',
          textTransform: 'uppercase',
          letterSpacing: '0.05em',
        }}
      >
        Packet Analyzer
      </div>
      <div style={{ overflowX: 'auto', maxHeight: '240px', overflowY: 'auto' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '11px' }}>
          <thead>
            <tr style={{ background: '#0f1117', position: 'sticky', top: 0, zIndex: 1 }}>
              <th style={{ padding: '4px 6px', textAlign: 'left', color: '#6b7280', fontWeight: 'normal' }}>SEQ</th>
              <th style={{ padding: '4px 6px', textAlign: 'left', color: '#6b7280', fontWeight: 'normal' }}>TYPE</th>
              <th style={{ padding: '4px 6px', textAlign: 'left', color: '#6b7280', fontWeight: 'normal' }}>DIR</th>
              <th style={{ padding: '4px 6px', textAlign: 'left', color: '#6b7280', fontWeight: 'normal' }}>SIZE</th>
              <th style={{ padding: '4px 6px', textAlign: 'left', color: '#6b7280', fontWeight: 'normal' }}>AGE</th>
              <th style={{ padding: '4px 6px', textAlign: 'left', color: '#6b7280', fontWeight: 'normal' }}>DECODED</th>
            </tr>
          </thead>
          <tbody>
            {packets.length === 0 ? (
              <tr>
                <td colSpan={6} style={{ padding: '10px 6px', color: '#6b7280', textAlign: 'center' }}>
                  Waiting for packets...
                </td>
              </tr>
            ) : (
              packets.map((p, i) => <PacketRow key={`${p.seq ?? '--'}-${p.tsMs}-${i}`} packet={p} />)
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
