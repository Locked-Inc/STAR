import { useEffect, useState } from 'react';
import { getRecentPackets } from '../services/GatewayService';
import type { PacketRecord } from '../services/GatewayService';

const MS_PER_SECOND = 1000;
const POLL_INTERVAL_MS = 200;
const MAX_PACKETS = 30;

const DIR_COLORS = { rx: '#4ade80', tx: '#38bdf8' } as const;
const TYPE_COLORS: Record<string, string> = {
  telemetry: '#c084fc',
  motors: '#fbbf24',
  battery: '#34d399',
  odometry: '#60a5fa',
  lidar: '#f472b6',
  alert: '#f87171',
  system: '#94a3b8',
  controller: '#818cf8',
  estop: '#ef4444',
  DECODE_ERROR: '#ef4444',
};

function ageStr(tsMs: number): string {
  const ms = Date.now() - tsMs;
  if (ms < MS_PER_SECOND) return `${ms}ms`;
  return `${(ms / MS_PER_SECOND).toFixed(1)}s`;
}

function PacketRow({ packet }: { packet: PacketRecord }) {
  const typeColor = TYPE_COLORS[packet.type] ?? '#94a3b8';
  const dirColor = DIR_COLORS[packet.direction];

  return (
    <tr className="packet-row">
      <td style={{ padding: '8px 12px', color: '#94a3b8' }}>
        {packet.seq || '--'}
      </td>
      <td style={{ padding: '8px 12px', color: typeColor }}>
        {packet.type}
      </td>
      <td style={{ padding: '8px 12px', color: dirColor, fontWeight: 600 }}>
        {packet.direction.toUpperCase()}
      </td>
      <td style={{ padding: '8px 12px', color: '#94a3b8' }}>
        {packet.sizeBytes}B
      </td>
      <td style={{ padding: '8px 12px', color: '#64748b' }}>
        {ageStr(packet.tsMs)}
      </td>
      <td
        className="data-flash"
        key={packet.preview}
        style={{
          padding: '8px 12px',
          color: '#f8fafc',
          maxWidth: '240px',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          whiteSpace: 'nowrap',
          fontFamily: 'SF Pro Display, sans-serif'
        }}
        title={packet.preview}
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

  const styles = `
    @keyframes slideDownPacket {
      from { opacity: 0; transform: translateY(-8px); }
      to { opacity: 1; transform: translateY(0); }
    }
    @keyframes pulseEllipsis {
      0% { opacity: 0.2; }
      50% { opacity: 1; }
      100% { opacity: 0.2; }
    }
    .packet-row {
      animation: slideDownPacket 0.2s cubic-bezier(0.25, 1, 0.5, 1) forwards;
      border-bottom: 0.5px solid rgba(255, 255, 255, 0.05);
      font-size: 12px;
      font-variant-numeric: tabular-nums;
    }
    .packet-row:nth-child(even) {
      background: rgba(255, 255, 255, 0.02);
    }
    .sticky-header th {
      position: sticky;
      top: 0;
      z-index: 2;
      background: rgba(13, 17, 23, 0.7);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border-bottom: 0.5px solid rgba(255, 255, 255, 0.15);
      padding: 10px 12px;
      text-align: left;
      color: rgba(255,255,255,0.6);
      font-weight: 600;
      font-size: 11px;
      letter-spacing: 0.05em;
    }
    .pulse-text {
      animation: pulseEllipsis 2s infinite ease-in-out;
    }
  `;

  return (
    <>
      <style>{styles}</style>
      <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
        <div
          className="panel-header"
          style={{
            padding: '16px 24px 8px 24px',
            fontSize: '11px',
            fontWeight: 600,
            color: 'rgba(255, 255, 255, 0.5)',
            textTransform: 'uppercase',
            letterSpacing: '0.12em',
            userSelect: 'none',
          }}
        >
          Packet Analyzer
        </div>
        <div className="panel-body" style={{ borderBottomLeftRadius: 0, borderBottomRightRadius: 0 }}>
          <table style={{ width: '100%', borderCollapse: 'collapse', textAlign: 'left' }}>
            <thead>
              <tr className="sticky-header">
                <th>SEQ</th>
                <th>TYPE</th>
                <th>DIR</th>
                <th>SIZE</th>
                <th>AGE</th>
                <th>DECODED</th>
              </tr>
            </thead>
            <tbody>
              {packets.length === 0 ? (
                <tr>
                  <td colSpan={6} style={{ padding: '40px 20px', color: '#64748b', textAlign: 'center', fontSize: '13px' }}>
                    <span className="pulse-text">Waiting for packets...</span>
                  </td>
                </tr>
              ) : (
                packets.map((p, i) => <PacketRow key={`${p.seq ?? '--'}-${p.tsMs}-${i}`} packet={p} />)
              )}
            </tbody>
          </table>
        </div>
      </div>
    </>
  );
}
