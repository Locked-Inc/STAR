import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import { COLORS } from '../theme';
import { MV_PER_V } from '../services/GatewayService';

const DECI_C_TO_C = 10;

const getBatteryColor = (soc: number) => {
  if (soc > 50) return '#30D158'; // System Green
  if (soc > 20) return '#FF9F0A'; // System Orange
  return '#FF453A'; // System Red
};

const SegmentedBatteryIcon = ({ soc, dimmed = false }: { soc: number; dimmed?: boolean }) => {
  const color = dimmed ? COLORS.textDim : getBatteryColor(soc);
  const segments = dimmed ? 0 : Math.max(1, Math.ceil(soc / 20)); // 1 to 5 segments, 0 if dimmed
  const shadowColor = `color-mix(in srgb, ${getBatteryColor(soc)} 65%, ${COLORS.panelBg})`;

  return (
    <svg aria-hidden="true" width="64" height="32" viewBox="0 0 64 32" fill="none" xmlns="http://www.w3.org/2000/svg" style={{ filter: dimmed ? 'none' : `drop-shadow(0 0 8px ${shadowColor})` }}>
      {/* Battery Body */}
      <rect x="2" y="2" width="54" height="28" rx="6" stroke={COLORS.border} strokeWidth="2" />
      {/* Battery Terminal */}
      <path d="M58 10H60C61.1046 10 62 10.8954 62 12V20C62 21.1046 61.1046 22 60 22H58V10Z" fill={COLORS.textMuted} />

      {/* Inner Segments */}
      <g style={{ transition: 'fill 0.3s ease' }} fill={color}>
        {segments >= 1 && <rect x="6" y="6" width="8" height="20" rx="2" />}
        {segments >= 2 && <rect x="16" y="6" width="8" height="20" rx="2" />}
        {segments >= 3 && <rect x="26" y="6" width="8" height="20" rx="2" />}
        {segments >= 4 && <rect x="36" y="6" width="8" height="20" rx="2" />}
        {segments >= 5 && <rect x="46" y="6" width="8" height="20" rx="2" />}
      </g>
    </svg>
  );
};

export function BatteryPanel() {
  const battery = useDashboardStore(useShallow((s) => s.battery));

  const packV = battery?.cells?.packMv != null
    ? (battery.cells.packMv / MV_PER_V).toFixed(2)
    : '--';

  const socPercentRaw = battery?.soc?.relativeSocPercent;
  const socPercent = socPercentRaw != null
    ? socPercentRaw.toFixed(0)
    : '--';

  const tempDC = battery?.temperatures?.tempDeciCelsius[0];
  const tempC = tempDC != null ? (tempDC / DECI_C_TO_C).toFixed(1) : '--';

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div
        className="panel-header"
        style={{
          padding: '16px 24px 12px 24px',
          fontSize: '11px',
          fontWeight: 600,
          color: 'rgba(255, 255, 255, 0.5)',
          textTransform: 'uppercase',
          letterSpacing: '0.12em',
          userSelect: 'none',
        }}
      >
        Battery
      </div>

      <div className="panel-body" style={{ padding: '4px 24px 24px 24px', display: 'flex', flexDirection: 'column', gap: '20px' }}>

        {battery == null ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '16px', opacity: 0.6 }}>
            <SegmentedBatteryIcon soc={0} dimmed />
            <span style={{ fontSize: '13px', color: 'rgba(255,255,255,0.6)' }}>No battery data</span>
          </div>
        ) : (
          <>
            {/* Top row with large SVG icon */}
            <div style={{ display: 'flex', justifyContent: 'center', marginBottom: '8px' }}>
              <SegmentedBatteryIcon soc={socPercentRaw || 0} />
            </div>

            {/* Data Rows */}
            <Row icon="V" label="Pack Voltage" value={`${packV} V`} />
            <Row icon="SOC" label="State of Charge" value={`${socPercent}%`} color={getBatteryColor(socPercentRaw || 0)} />
            <Row icon="TEMP" label="Temperature" value={`${tempC} deg C`} />
          </>
        )}

      </div>
    </div>
  );
}

function Row({ icon, label, value, color = '#FFFFFF' }: { icon: string; label: string; value: string; color?: string }) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <span style={{ fontSize: '14px', opacity: 0.8 }}>{icon}</span>
        <span style={{ fontSize: '12px', color: 'rgba(255,255,255,0.65)', fontWeight: 500 }}>{label}</span>
      </div>
      <div
        className="data-flash"
        key={`${label}-${value}`}
        style={{ fontSize: '22px', fontWeight: 300, color, fontVariantNumeric: 'tabular-nums' }}
      >
        {value}
      </div>
    </div>
  );
}
