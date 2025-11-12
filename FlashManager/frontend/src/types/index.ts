export type UserRole = 'USER' | 'ADMIN';

export type DeviceStatus = 'ONLINE' | 'OFFLINE' | 'BUSY';

export type Platform = 'MAC' | 'WINDOWS' | 'LINUX' | 'WSL2';

export type DeviceCapability = 'ESP32' | 'PI5' | 'ANDROID';

export type Component = 'ESP32_FIRMWARE' | 'PI5_OS' | 'ANDROID_APP' | 'ROBOT_GATEWAY' | 'SERVER_BACKEND';

export type JobStatus = 'PENDING' | 'RUNNING' | 'COMPLETED' | 'FAILED' | 'CANCELLED';

export type LogLevel = 'INFO' | 'WARN' | 'ERROR' | 'DEBUG';

export type FlashResult = 'SUCCESS' | 'FAILED' | 'TIMEOUT';

export interface User {
  id: string;
  email: string;
  role: UserRole;
  createdAt: string;
}

export interface Device {
  id: string;
  name: string;
  userId: string;
  status: DeviceStatus;
  lastSeen?: string;
  platform: Platform;
  capabilities: DeviceCapability[];
  createdAt: string;
  updatedAt: string;
}

export interface BuildJob {
  id: string;
  component: Component;
  branch: string;
  status: JobStatus;
  startedAt?: string;
  completedAt?: string;
  binaryPath?: string;
  binarySize?: number;
  buildDuration?: number;
  exitCode?: number;
  createdAt: string;
  updatedAt: string;
}

export interface BuildLog {
  id: string;
  buildJobId: string;
  timestamp: string;
  line: string;
  level: LogLevel;
}

export interface FlashJob {
  id: string;
  targetDevice: string;
  status: JobStatus;
  priority: number;
  claimedAt?: string;
  completedAt?: string;
  createdAt: string;
  updatedAt: string;
  device: {
    id: string;
    name: string;
  };
  buildJob: {
    id: string;
    component: Component;
    branch: string;
  };
}

export interface FlashHistory {
  id: string;
  result: FlashResult;
  duration: number;
  errorMessage?: string;
  createdAt: string;
  flashJob: {
    id: string;
    targetDevice: string;
    device: {
      name: string;
    };
    buildJob: {
      component: Component;
      branch: string;
    };
  };
}

export interface AuthPayload {
  accessToken: string;
  refreshToken: string;
  user?: User;
  device?: Device;
}

export interface DeviceRegistration {
  device: Device;
  apiKey: string;
}
