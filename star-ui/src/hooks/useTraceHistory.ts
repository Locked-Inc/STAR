import { useEffect, useRef, useState } from 'react';
import { pushTraceSample, traceHistoryLength, traceSampleIntervalMs } from '../lib/dashboard';
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
    cpu: Array(traceHistoryLength).fill(Number.NaN),
    lidar: Array(traceHistoryLength).fill(Number.NaN),
    temperature: Array(traceHistoryLength).fill(Number.NaN),
    velocity: Array(traceHistoryLength).fill(Number.NaN),
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
     }, traceSampleIntervalMs);

    return () => window.clearInterval(interval);
  }, []);

  return traceHistory;
}
