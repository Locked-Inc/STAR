import type { MouseEvent as ReactMouseEvent, ReactNode } from 'react';
import type { AppRoute, Tone } from '../../types/dashboard';

interface GlyphProps {
  accent: Tone;
  label: string;
}

export function Glyph({ accent, label }: GlyphProps) {
  return (
    <div className={`star-glyph star-glyph--${accent}`} aria-hidden="true">
      {label}
    </div>
  );
}

interface StatusDotProps {
  tone: Tone;
}

export function StatusDot({ tone }: StatusDotProps) {
  return <span className={`status-dot status-dot--${tone}`} />;
}

interface ChipProps {
  tone?: Tone;
  children: ReactNode;
}

export function Chip({ tone = 'neutral', children }: ChipProps) {
  return <span className={`ui-chip ui-chip--${tone}`}>{children}</span>;
}

interface MetricTileProps {
  label: string;
  value: string;
  tone?: Tone;
  detail?: string;
}

export function MetricTile({ label, value, tone = 'neutral', detail }: MetricTileProps) {
  return (
    <div className="metric-tile">
      <span className="metric-label">{label}</span>
      <span className={`metric-value metric-value--${tone}`}>{value}</span>
      {detail ? <span className="metric-detail">{detail}</span> : null}
    </div>
  );
}

interface HealthRowProps {
  label: string;
  tone: Tone;
  value: string;
}

export function HealthRow({ label, tone, value }: HealthRowProps) {
  return (
    <div className="health-row">
      <div className="health-row__label">
        <StatusDot tone={tone} />
        <span>{label}</span>
      </div>
      <span className={`health-row__value health-row__value--${tone}`}>{value}</span>
    </div>
  );
}

interface HeaderLinkProps {
  href: AppRoute;
  onNavigate: (route: AppRoute) => void;
  label: string;
  accent: Tone;
  glyph: string;
}

export function HeaderLink({ href, onNavigate, label, accent, glyph }: HeaderLinkProps) {
  function handleClick(event: ReactMouseEvent<HTMLAnchorElement>): void {
    event.preventDefault();
    onNavigate(href);
  }

  return (
    <a className="header-link" href={href} onClick={handleClick}>
      <Glyph accent={accent} label={glyph} />
      <span>{label}</span>
    </a>
  );
}

interface ControlButtonProps {
  label: string;
  tone: Tone;
  onClick: () => void | Promise<void>;
  disabled?: boolean;
}

export function ControlButton({ label, tone, onClick, disabled }: ControlButtonProps) {
  return (
    <button
      className={`control-button control-button--${tone}`}
      disabled={disabled}
      type="button"
      onClick={() => {
        void onClick();
      }}
    >
      {label}
    </button>
  );
}
