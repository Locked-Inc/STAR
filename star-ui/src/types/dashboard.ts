export type AppRoute = '/' | '/ros';

export type UiMode = 'autonomous' | 'manual';

export type Tone = 'neutral' | 'good' | 'warn' | 'danger' | 'accent';

export interface LogLine {
  key: string;
  tsMs: number;
  tone: Tone;
  source: string;
  message: string;
  emphasis: string;
}

export interface TraceHistory {
  cpu: number[];
  lidar: number[];
  temperature: number[];
  velocity: number[];
}

export interface RosTopicDefinition {
  packetType: string;
  topic: string;
  type: string;
  description: string;
}
