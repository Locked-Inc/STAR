import { clamp } from '../../lib/dashboard';

const ROLL_VISUAL_GAIN = 0.65;
const PITCH_VISUAL_GAIN = 0.42;
const ROLL_VISUAL_LIMIT = 22;
const PITCH_VISUAL_LIMIT = 7;
const VIEWBOX_CENTER_X = 50;
const VIEWBOX_CENTER_Y = 50;
const DECK_WIDTH = 40;
const DECK_HEIGHT = 30;
const DECK_X = -DECK_WIDTH / 2;
const DECK_Y_OFFSET = -DECK_HEIGHT / 2;
const DECK_TOP_RAISE = 10;
const DECK_TOP_EXTEND = 10;
const DECK_TOP_LEFT_X = DECK_X + DECK_TOP_EXTEND;
const DECK_TOP_RIGHT_X = DECK_X + DECK_WIDTH - DECK_TOP_EXTEND;
const DECK_RIGHT_X = DECK_X + DECK_WIDTH;
const DECK_BOTTOM_Y = DECK_Y_OFFSET + DECK_HEIGHT;
const DECK_TOP_Y = DECK_Y_OFFSET;
const DECK_ROOF_Y = DECK_Y_OFFSET - DECK_TOP_RAISE;
const DECK_SIDE_BOTTOM_Y = DECK_BOTTOM_Y - DECK_TOP_RAISE;
const ARROW_BASE_WIDTH = 10;
const ARROW_BASE_Y = -20;
const ARROW_TIP_OFFSET = -10;
const ARROW_CENTER_X_RATIO = 0.75;
const ARROW_CENTER_X = DECK_X + DECK_WIDTH * ARROW_CENTER_X_RATIO;
const ARROW_LEFT_X = ARROW_CENTER_X - ARROW_BASE_WIDTH / 2;
const ARROW_RIGHT_X = ARROW_CENTER_X + ARROW_BASE_WIDTH / 2;
const ARROW_PITCH_SCALE = 0.35;

interface ImuOrientationGraphicProps {
  pitchDeg: number;
  rollDeg: number;
}

export function ImuOrientationGraphic({ pitchDeg, rollDeg }: ImuOrientationGraphicProps) {
  const safePitchDeg = Number.isFinite(pitchDeg) ? pitchDeg : 0;
  const safeRollDeg = Number.isFinite(rollDeg) ? rollDeg : 0;
  const visualRoll = clamp(safeRollDeg * ROLL_VISUAL_GAIN, -ROLL_VISUAL_LIMIT, ROLL_VISUAL_LIMIT);
  const visualPitch = clamp(safePitchDeg * PITCH_VISUAL_GAIN, -PITCH_VISUAL_LIMIT, PITCH_VISUAL_LIMIT);
  const deckY = VIEWBOX_CENTER_Y + visualPitch;
  const arrowTipY = ARROW_BASE_Y + ARROW_TIP_OFFSET - visualPitch * ARROW_PITCH_SCALE;

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

        <g transform={`translate(${VIEWBOX_CENTER_X} ${deckY}) rotate(${visualRoll})`}>
          <rect
            fill="url(#imu-front)"
            height={DECK_HEIGHT}
            rx="2"
            stroke="#60a5fa"
            strokeWidth="1"
            width={DECK_WIDTH}
            x={DECK_X}
            y={DECK_Y_OFFSET}
          />
          <path
            d={`M${DECK_X},${DECK_TOP_Y} L${DECK_TOP_LEFT_X},${DECK_ROOF_Y} L${DECK_RIGHT_X},${DECK_ROOF_Y} L${DECK_TOP_RIGHT_X},${DECK_TOP_Y} Z`}
            fill="#60a5fa"
            stroke="#93c5fd"
            strokeWidth="1"
          />
          <path
            d={`M${DECK_TOP_RIGHT_X},${DECK_TOP_Y} L${DECK_RIGHT_X},${DECK_ROOF_Y} L${DECK_RIGHT_X},${DECK_SIDE_BOTTOM_Y} L${DECK_TOP_RIGHT_X},${DECK_BOTTOM_Y} Z`}
            fill="#2563eb"
            stroke="#3b82f6"
            strokeWidth="1"
          />
          <path d={`M${ARROW_LEFT_X},${ARROW_BASE_Y} L${ARROW_RIGHT_X},${ARROW_BASE_Y} L${ARROW_CENTER_X},${arrowTipY} Z`} fill="#ef4444" />
        </g>

        <line stroke="#1e293b" strokeDasharray="3 3" strokeWidth="1" x1="10" x2="90" y1="50" y2="50" />
        <line stroke="#1e293b" strokeDasharray="3 3" strokeWidth="1" x1="50" x2="50" y1="10" y2="90" />
      </svg>
      <span className="imu-orientation__label">Visual Orientation</span>
    </div>
  );
}
