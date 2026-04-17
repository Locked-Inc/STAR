import { useEffect, useRef, useState } from 'react';
import { coverageStepPercent, coverageTickIntervalMs } from '../lib/dashboard';
import { AlertLevel } from '../proto/star/v1/ui';
import { useDashboardStore } from '../store/dashboardStore';
import type { Tone, UiMode } from '../types/dashboard';

interface EStopResumeSnapshot {
  coveragePercent: number;
  selectedMode: UiMode;
  taskCompleted: boolean;
  wasAutonomyRequested: boolean;
}

interface UseOperatorControlsArgs {
  demoMode: boolean;
  eStopActive: boolean;
  reportedMode: UiMode | null;
  sendEStop: (reason: string) => void;
  sendEStopRelease: () => Promise<boolean>;
}

interface OperatorControls {
  autonomyRunning: boolean;
  coveragePercent: number;
  handleAutonomyStart: () => void;
  handleAutonomyStop: () => void;
  handleEStopToggle: () => Promise<void>;
  handleModeToggle: () => void;
  modeChangeDisabled: boolean;
  missionProgressAvailable: boolean;
  statusCopy: string;
  statusTone: Tone;
  taskCompleted: boolean;
  uiMode: UiMode;
}

export function useOperatorControls({
  demoMode,
  eStopActive,
  reportedMode,
  sendEStop,
  sendEStopRelease,
}: UseOperatorControlsArgs): OperatorControls {
  const [selectedMode, setSelectedMode] = useState<UiMode>('autonomous');
  const [autonomyRequested, setAutonomyRequested] = useState(false);
  const [coveragePercent, setCoveragePercent] = useState(0);
  const [taskCompleted, setTaskCompleted] = useState(false);
  const estopResumeRef = useRef<EStopResumeSnapshot | null>(null);

  const uiMode = reportedMode ?? selectedMode;
  const modeChangeDisabled = eStopActive || reportedMode !== null;
  const missionProgressAvailable = demoMode;
  const autonomyRunning = demoMode && autonomyRequested && uiMode === 'autonomous' && !eStopActive && !taskCompleted;

  useEffect(() => {
    if (!autonomyRunning) {
      return;
    }

    const interval = window.setInterval(() => {
      setCoveragePercent((currentCoverage) => {
        const nextCoverage = Math.min(100, currentCoverage + coverageStepPercent);
        if (nextCoverage >= 100) {
          setTaskCompleted(true);
          setAutonomyRequested(false);
        }
        return nextCoverage;
      });
    }, coverageTickIntervalMs);

    return () => window.clearInterval(interval);
  }, [autonomyRunning]);

  async function handleEStopToggle(): Promise<void> {
    const store = useDashboardStore.getState();

    if (eStopActive) {
      const released = await sendEStopRelease();
      const snapshot = estopResumeRef.current;

      store.releaseEStop();

      if (snapshot) {
        setSelectedMode(snapshot.selectedMode);
        setCoveragePercent(snapshot.coveragePercent);
        setTaskCompleted(snapshot.taskCompleted);
        setAutonomyRequested(snapshot.wasAutonomyRequested);
      }

      if (!released) {
        store.addAlert({
          code: 'ESTOP_RESUME_LOCAL',
          level: AlertLevel.WARN,
          message: 'Resume applied locally; backend release confirmation was not received.',
          source: 'ui',
          timestampUs: String(Date.now() * 1000),
        });
      }
      return;
    }

    estopResumeRef.current = {
      coveragePercent,
      selectedMode,
      taskCompleted,
      wasAutonomyRequested: autonomyRequested,
    };
    setAutonomyRequested(false);
    sendEStop('user_ui_button');
    store.triggerEStop();
  }

  function handleModeToggle(): void {
    if (modeChangeDisabled) {
      return;
    }

    setSelectedMode((currentMode) => {
      const nextMode: UiMode = currentMode === 'autonomous' ? 'manual' : 'autonomous';
      if (nextMode === 'manual') {
        setAutonomyRequested(false);
      } else {
        setTaskCompleted(false);
        setCoveragePercent(0);
      }
      return nextMode;
    });
  }

  function handleAutonomyStart(): void {
    if (!demoMode) {
      useDashboardStore.getState().addAlert({
        code: 'MISSION_PROGRESS_UNAVAILABLE',
        level: AlertLevel.INFO,
        message: 'Live mission progress is unavailable without an operator demo feed.',
        source: 'ui',
        timestampUs: String(Date.now() * 1000),
      });
      return;
    }

    setTaskCompleted(false);
    setCoveragePercent(0);
    setAutonomyRequested(true);
  }

  function handleAutonomyStop(): void {
    setAutonomyRequested(false);
  }

  const statusCopy = eStopActive
    ? 'Emergency Stopped'
    : taskCompleted
      ? 'Completed'
      : autonomyRunning
        ? 'Running'
        : 'Standby';

  const statusTone: Tone = eStopActive ? 'danger' : taskCompleted ? 'good' : autonomyRunning ? 'accent' : 'neutral';

  return {
    autonomyRunning,
    coveragePercent,
    handleAutonomyStart,
    handleAutonomyStop,
    handleEStopToggle,
    handleModeToggle,
    missionProgressAvailable,
    modeChangeDisabled,
    statusCopy,
    statusTone,
    taskCompleted,
    uiMode,
  };
}
