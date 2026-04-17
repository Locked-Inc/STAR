import type { Tone } from '../types/dashboard';

export type TraceAccent = Tone;
interface DebugTraceChartProps {
  accent: TraceAccent;
  formatValue: (value: number) => string;
  max?: number;
  min?: number;
  samples: number[];
  title: string;
}

function buildPath(samples: number[], min: number, max: number, width: number, height: number): string {
  if (samples.length === 0) {
    return '';
  }

  const span = max - min;

  if (span <= 0) {
    return '';
  }

  return samples
    .map((sample, index) => {
      const x = (index / Math.max(samples.length - 1, 1)) * width;
      const normalized = (sample - min) / span;
      const y = height - normalized * height;
      return `${index === 0 ? 'M' : 'L'} ${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(' ');
}

const chartWidth = 280;
const chartHeight = 110;

export function DebugTraceChart({
  accent,
  formatValue,
  max,
  min,
  samples,
  title,
}: DebugTraceChartProps) {
  const plottedSamples = samples.filter((sample) => Number.isFinite(sample));
  const safeSamples = plottedSamples.length > 0 ? plottedSamples : [0];
  const sampleMin = Math.min(...safeSamples);
  const sampleMax = Math.max(...safeSamples);

  if (min !== undefined && max !== undefined && min >= max) {
    throw new Error('DebugTraceChart expects min < max when both bounds are provided.');
  }

  const computedMin =
    min !== undefined ? Math.min(Math.max(min, sampleMin), sampleMax) : sampleMin;
  let computedMax =
    max !== undefined ? Math.min(Math.max(max, sampleMin), sampleMax) : sampleMax;

  if (computedMax <= computedMin) {
    computedMax = computedMin + 1;
  }
  const currentValue = plottedSamples[plottedSamples.length - 1] ?? 0;
  const path = buildPath(safeSamples, computedMin, computedMax, chartWidth, chartHeight);
  const areaPath = `${path} L ${chartWidth} ${chartHeight} L 0 ${chartHeight} Z`;

  return (
    <div className="trace-card">
      <div className="trace-card__header">
        <div>
          <h3>{title}</h3>
          <p>{formatValue(currentValue)}</p>
        </div>
        <span className={`trace-card__badge trace-card__badge--${accent}`}>{plottedSamples.length} pts</span>
      </div>

      <svg aria-hidden="true" className="trace-card__svg" viewBox={`0 0 ${chartWidth} ${chartHeight}`}>
        {[0.25, 0.5, 0.75].map((ratio) => (
          <line
            key={ratio}
            className="trace-card__grid-line"
            x1="0"
            x2={chartWidth}
            y1={(chartHeight * ratio).toFixed(2)}
            y2={(chartHeight * ratio).toFixed(2)}
          />
        ))}
        <path className={`trace-card__area trace-card__area--${accent}`} d={areaPath} />
        <path className={`trace-card__line trace-card__line--${accent}`} d={path} />
      </svg>

      <div className="trace-card__footer">
        <span>{formatValue(computedMin)}</span>
        <span>{formatValue(computedMax)}</span>
      </div>
    </div>
  );
}
