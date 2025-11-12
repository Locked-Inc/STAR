import { gql } from '@apollo/client';

// Auth Mutations
export const LOGIN_MUTATION = gql`
  mutation Login($input: LoginInput!) {
    login(input: $input) {
      accessToken
      refreshToken
      user {
        id
        email
        role
      }
    }
  }
`;

export const REGISTER_MUTATION = gql`
  mutation Register($input: RegisterInput!) {
    register(input: $input) {
      accessToken
      refreshToken
      user {
        id
        email
        role
      }
    }
  }
`;

// User Queries
export const ME_QUERY = gql`
  query Me {
    me {
      id
      email
      role
      createdAt
    }
  }
`;

// Device Queries
export const DEVICES_QUERY = gql`
  query Devices {
    devices {
      id
      name
      status
      platform
      capabilities
      lastSeen
      createdAt
    }
  }
`;

export const MY_DEVICES_QUERY = gql`
  query MyDevices {
    myDevices {
      id
      name
      status
      platform
      capabilities
      lastSeen
      createdAt
    }
  }
`;

// Device Mutations
export const REGISTER_DEVICE_MUTATION = gql`
  mutation RegisterDevice($input: RegisterDeviceInput!) {
    registerDevice(input: $input) {
      device {
        id
        name
        platform
        capabilities
      }
      apiKey
    }
  }
`;

// Build Queries
export const BUILD_JOBS_QUERY = gql`
  query BuildJobs($status: JobStatus, $component: Component, $limit: Int) {
    buildJobs(status: $status, component: $component, limit: $limit) {
      id
      component
      branch
      status
      startedAt
      completedAt
      binaryPath
      binarySize
      buildDuration
      exitCode
      createdAt
      updatedAt
    }
  }
`;

export const BUILD_JOB_QUERY = gql`
  query BuildJob($id: ID!) {
    buildJob(id: $id) {
      id
      component
      branch
      status
      startedAt
      completedAt
      binaryPath
      binarySize
      buildDuration
      exitCode
      createdAt
      updatedAt
    }
  }
`;

export const BUILD_LOGS_QUERY = gql`
  query BuildLogs($buildJobId: ID!, $limit: Int, $offset: Int) {
    buildLogs(buildJobId: $buildJobId, limit: $limit, offset: $offset) {
      id
      timestamp
      line
      level
    }
  }
`;

// Build Mutations
export const TRIGGER_BUILD_MUTATION = gql`
  mutation TriggerBuild($input: TriggerBuildInput!) {
    triggerBuild(input: $input) {
      id
      component
      branch
      status
      createdAt
    }
  }
`;

export const CANCEL_BUILD_MUTATION = gql`
  mutation CancelBuild($buildJobId: ID!) {
    cancelBuild(buildJobId: $buildJobId)
  }
`;

// Flash Queries
export const FLASH_JOBS_QUERY = gql`
  query FlashJobs($status: JobStatus, $deviceId: ID, $limit: Int) {
    flashJobs(status: $status, deviceId: $deviceId, limit: $limit) {
      id
      targetDevice
      status
      priority
      claimedAt
      completedAt
      createdAt
      device {
        id
        name
      }
      buildJob {
        id
        component
        branch
      }
    }
  }
`;

export const FLASH_HISTORY_QUERY = gql`
  query FlashHistory($limit: Int) {
    flashHistory(limit: $limit) {
      id
      result
      duration
      errorMessage
      createdAt
      flashJob {
        id
        targetDevice
        device {
          name
        }
        buildJob {
          component
          branch
        }
      }
    }
  }
`;

// Flash Mutations
export const CREATE_FLASH_JOB_MUTATION = gql`
  mutation CreateFlashJob($input: CreateFlashJobInput!) {
    createFlashJob(input: $input) {
      id
      status
      priority
      createdAt
    }
  }
`;

export const CANCEL_FLASH_JOB_MUTATION = gql`
  mutation CancelFlashJob($flashJobId: ID!) {
    cancelFlashJob(flashJobId: $flashJobId)
  }
`;

// Subscriptions
export const BUILD_JOB_UPDATED_SUBSCRIPTION = gql`
  subscription BuildJobUpdated($buildJobId: ID!) {
    buildJobUpdated(buildJobId: $buildJobId) {
      id
      status
      startedAt
      completedAt
      binaryPath
      exitCode
    }
  }
`;

export const BUILD_LOG_STREAM_SUBSCRIPTION = gql`
  subscription BuildLogStream($buildJobId: ID!) {
    buildLogStream(buildJobId: $buildJobId) {
      id
      timestamp
      line
      level
    }
  }
`;

export const FLASH_JOB_UPDATED_SUBSCRIPTION = gql`
  subscription FlashJobUpdated($deviceId: ID!) {
    flashJobUpdated(deviceId: $deviceId) {
      id
      status
      claimedAt
      completedAt
    }
  }
`;
