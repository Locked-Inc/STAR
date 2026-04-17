import { clamp } from '../../lib/dashboard';

interface ImuOrientationGraphicProps {
  pitchDeg: number;
  rollDeg: number;
}

export function ImuOrientationGraphic({ pitchDeg, rollDeg }: ImuOrientationGraphicProps) {
  const visualRoll = clamp(rollDeg * 0.65, -22, 22);
  const visualPitch = clamp(pitchDeg * 0.42, -7, 7);
  const deckY = 50 + visualPitch;
  const arrowY = -30 - visualPitch * 0.35;

  return (
    <div className="imu-orientation">
      <svg aria-hidden="true" className="imu-orientation__svg" viewBox="0 0 100 100">
        <defs>
          <linearGradient id="imu-front" x1="0" x2="1">
            <stop offset="0%" stopColor="#3b82f6" />
            <stop offset="100%" stopColor="#2563eb" />
          </linearGradient>
        </defs>

        <circle cx="50" cy="50" fill="none" r="40" stroke="#1e293b" strokeWidth="1" />
        <ellipse cx="50" cy="75" fill="#334155" opacity="0.3" rx="35" ry="10" />

        <g transform={`translate(50 ${deckY}) rotate(${visualRoll})`}>
          <rect fill="url(#imu-front)" height="30" rx="2" stroke="#60a5fa" strokeWidth="1" width="40" x="-20" y="-15" />
          <path d="M-20,-15 L-10,-25 L30,-25 L20,-15 Z" fill="#60a5fa" stroke="#93c5fd" strokeWidth="1" />
          <path d="M20,-15 L30,-25 L30,5 L20,15 Z" fill="#2563eb" stroke="#3b82f6" strokeWidth="1" />
          <path d={`M5,-20 L15,-20 L10,${arrowY} Z`} fill="#ef4444" />
        </g>

        <line stroke="#1e293b" strokeDasharray="3 3" strokeWidth="1" x1="10" x2="90" y1="50" y2="50" />
        <line stroke="#1e293b" strokeDasharray="3 3" strokeWidth="1" x1="50" x2="50" y1="10" y2="90" />
      </svg>
      <span className="imu-orientation__label">Visual Orientation</span>
    </div>
  );
}
