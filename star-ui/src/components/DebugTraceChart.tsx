type TraceAccent = 'good' | 'warn' | 'accent' | 'danger' | 'neutral';

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

  const span = Math.max(max - min, 1);

  return samples
    .map((sample, index) => {
      const x = (index / Math.max(samples.length - 1, 1)) * width;
      const normalized = (sample - min) / span;
      const y = height - normalized * height;
      return `${index === 0 ? 'M' : 'L'} ${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(' ');
}

export function DebugTraceChart({
  accent,
  formatValue,
  max,
  min,
  samples,
  title,
}: DebugTraceChartProps) {
  const chartWidth = 280;
  const chartHeight = 110;
  const safeSamples = samples.length > 0 ? samples : [0];
  const computedMin = min ?? Math.min(...safeSamples);
  const computedMax = max ?? Math.max(...safeSamples, computedMin + 1);
  const currentValue = safeSamples[safeSamples.length - 1] ?? 0;
  const path = buildPath(safeSamples, computedMin, computedMax, chartWidth, chartHeight);
  const areaPath = `${path} L ${chartWidth} ${chartHeight} L 0 ${chartHeight} Z`;

  return (
    <div className="trace-card">
      <div className="trace-card__header">
        <div>
          <h3>{title}</h3>
          <p>{formatValue(currentValue)}</p>
        </div>
        <span className={`trace-card__badge trace-card__badge--${accent}`}>{safeSamples.length} pts</span>
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
