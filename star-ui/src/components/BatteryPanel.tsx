import { useDashboardStore } from '../store/dashboardStore';

export function BatteryPanel() {
  const battery = useDashboardStore((s) => s.battery);

  const packV = battery?.cells?.packMv != null
    ? (battery.cells.packMv / 1000).toFixed(2)
    : '--';

  const socPercent = battery?.soc != null
    ? (battery.soc as unknown as { socPercent?: number }).socPercent?.toFixed(0) ?? '--'
    : '--';

  const tempDC = battery?.temperatures?.tempDeciCelsius[0];
  const tempC = tempDC != null ? (tempDC / 10).toFixed(1) : '--';

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
        Battery
      </div>
      <div style={{ padding: '10px', display: 'flex', flexDirection: 'column', gap: '6px' }}>
        <Row label="Pack Voltage" value={`${packV} V`} />
        <Row label="SOC" value={`${socPercent}%`} />
        <Row label="Temp" value={`${tempC} C`} />
        {battery?.cells != null && (
          <Row
            label="Cell Delta"
            value={`${battery.cells.deltaMv} mV`}
          />
        )}
      </div>
      {battery == null && (
        <div style={{ padding: '10px', color: '#6b7280', fontSize: '12px' }}>
          No battery data
        </div>
      )}
    </div>
  );
}

function Row({ label, value }: { label: string; value: string }) {
  return (
    <div
      style={{
        display: 'flex',
        justifyContent: 'space-between',
        fontSize: '12px',
      }}
    >
      <span style={{ color: '#9ca3af' }}>{label}</span>
      <span style={{ fontFamily: 'monospace', fontWeight: 'bold' }}>{value}</span>
    </div>
  );
}
