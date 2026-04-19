/**
 * @file useOperatorControls.ts
 * @brief Manage operator UI control state and action handlers.
 *
 * Centralizes local operator state (selected mode, coverage, task completion,
 * autonomy request) and the handlers that bridge it to the dashboard store
 * and the STAR WebSocket/REST control surface.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * @license MIT
*/

import { useEffect, useRef, useState } from 'react';
import { buildUiAlert, coverageStepPercent, coverageTickIntervalMs, estopReasonUserUiButton } from '../lib/dashboard';
import { AlertLevel } from '../proto/star/v1/ui';
import { useDashboardStore } from '../store/dashboardStore';
import type { Tone, UiMode } from '../types/dashboard';

interface EStopResumeSnapshot {
  coveragePercent: number;
  selectedMode: UiMode;
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
  const mountedRef = useRef<boolean>(false); // Flipped true on mount, false on unmount; gates post-await state writes.
  const [selectedMode, setSelectedMode] = useState<UiMode>('autonomous');
  const [autonomyRequested, setAutonomyRequested] = useState(false);
  const [coveragePercent, setCoveragePercent] = useState(0);
  const estopResumeRef = useRef<EStopResumeSnapshot | null>(null);

  const uiMode = reportedMode ?? selectedMode;
  const modeChangeDisabled = eStopActive || reportedMode !== null;
  const missionProgressAvailable = demoMode;
  const taskCompleted = coveragePercent >= 100;
  const autonomyRunning = demoMode && autonomyRequested && uiMode === 'autonomous' && !eStopActive && !taskCompleted;

  useEffect(() => {
    mountedRef.current = true;

    return () => {
      mountedRef.current = false;
    };
  }, []);

  useEffect(() => {
    if (!autonomyRunning) {
      return;
    }

    const interval = window.setInterval(() => {
      setCoveragePercent((currentCoverage) =>
        Math.min(100, currentCoverage + coverageStepPercent),
      );
    }, coverageTickIntervalMs);

    return () => window.clearInterval(interval);
  }, [autonomyRunning]);

  async function handleEStopToggle(): Promise<void> {
    const store = useDashboardStore.getState();

    if (eStopActive) {
      let released: boolean;
      try {
        released = await sendEStopRelease();
      } catch (error: unknown) {
        console.error('E-stop release failed:', error);
        if (mountedRef.current) {
          const message =
            error instanceof Error ? error.message : 'Unknown error';
          store.addAlert(
            buildUiAlert(
              'ESTOP_RELEASE_FAILED',
              AlertLevel.WARN,
              `E-stop release request failed: ${message}`,
            ),
          );
        }
        return;
      }

      if (!mountedRef.current) {
        return;
      }

      const snapshot = estopResumeRef.current;

      store.releaseEStop();

      if (snapshot) {
        setSelectedMode(snapshot.selectedMode);
        setCoveragePercent(snapshot.coveragePercent);
        setAutonomyRequested(snapshot.wasAutonomyRequested);
      }

      if (!released) {
        store.addAlert(
          buildUiAlert(
            'ESTOP_RESUME_LOCAL',
            AlertLevel.WARN,
            'Resume applied locally; backend release confirmation was not received.',
          ),
        );
      }
      return;
    }

    estopResumeRef.current = {
      coveragePercent,
      selectedMode,
      wasAutonomyRequested: autonomyRequested,
    };
    setAutonomyRequested(false);
    sendEStop(estopReasonUserUiButton);
    store.triggerEStop();
  }

  function handleModeToggle(): void {
    if (modeChangeDisabled) {
      return;
    }

    const nextMode: UiMode = selectedMode === 'autonomous' ? 'manual' : 'autonomous';
    setSelectedMode(nextMode);
    if (nextMode === 'manual') {
      setAutonomyRequested(false);
    } else {
      setCoveragePercent(0);
    }
  }

  function handleAutonomyStart(): void {
    if (!demoMode) {
      useDashboardStore.getState().addAlert(
        buildUiAlert(
          'MISSION_PROGRESS_UNAVAILABLE',
          AlertLevel.INFO,
          'Live mission progress is unavailable without an operator demo feed.',
        ),
      );
      return;
    }

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
