import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import { COLORS, PANEL_CONTAINER_STYLE, PANEL_HEADER_STYLE } from '../theme';

const MV_TO_V = 1000;
const DECI_C_TO_C = 10;

export function BatteryPanel() {
  const battery = useDashboardStore(useShallow((s) => s.battery));

  const packV = battery?.cells?.packMv != null
    ? (battery.cells.packMv / MV_TO_V).toFixed(2)
    : '--';

  const socPercent = battery?.soc?.relativeSocPercent != null
    ? battery.soc.relativeSocPercent.toFixed(0)
    : '--';

  const tempDC = battery?.temperatures?.tempDeciCelsius[0];
  const tempC = tempDC != null ? (tempDC / DECI_C_TO_C).toFixed(1) : '--';

  return (
    <div style={PANEL_CONTAINER_STYLE}>
      <div style={PANEL_HEADER_STYLE}>
        Battery
      </div>
      <div style={{ padding: '10px', display: 'flex', flexDirection: 'column', gap: '6px' }}>
        <Row label="Pack Voltage" value={`${packV} V`} />
        <Row label="SOC" value={`${socPercent}%`} />
        <Row label="Temp" value={`${tempC} C`} />
        {battery?.cells?.deltaMv != null && (
          <Row
            label="Cell Delta"
            value={`${battery.cells.deltaMv} mV`}
          />
        )}
      </div>
      {battery == null && (
        <div style={{ padding: '10px', color: COLORS.textDim, fontSize: '12px' }}>
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
      <span style={{ color: COLORS.textMuted }}>{label}</span>
      <span style={{ fontFamily: 'monospace', fontWeight: 'bold' }}>{value}</span>
    </div>
  );
}
