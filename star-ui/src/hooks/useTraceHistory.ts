import { useEffect, useRef, useState } from 'react';
import { pushTraceSample, traceHistoryLength } from '../lib/dashboard';
import type { TraceHistory } from '../types/dashboard';

interface TraceSamples {
  cpu: number;
  lidar: number;
  temperature: number;
  velocity: number;
}

export function useTraceHistory(samples: TraceSamples): TraceHistory {
  const latestTraceSamplesRef = useRef(samples);
  const [traceHistory, setTraceHistory] = useState<TraceHistory>(() => ({
    cpu: Array.from({ length: traceHistoryLength }, () => samples.cpu),
    lidar: Array.from({ length: traceHistoryLength }, () => samples.lidar),
    temperature: Array.from({ length: traceHistoryLength }, () => samples.temperature),
    velocity: Array.from({ length: traceHistoryLength }, () => samples.velocity),
  }));

  useEffect(() => {
    latestTraceSamplesRef.current = samples;
  }, [samples]);

  useEffect(() => {
    const interval = window.setInterval(() => {
      setTraceHistory((currentHistory) => ({
        cpu: pushTraceSample(currentHistory.cpu, latestTraceSamplesRef.current.cpu),
        lidar: pushTraceSample(currentHistory.lidar, latestTraceSamplesRef.current.lidar),
        temperature: pushTraceSample(currentHistory.temperature, latestTraceSamplesRef.current.temperature),
        velocity: pushTraceSample(currentHistory.velocity, latestTraceSamplesRef.current.velocity),
      }));
    }, 1000);

    return () => window.clearInterval(interval);
  }, []);

  return traceHistory;
}
