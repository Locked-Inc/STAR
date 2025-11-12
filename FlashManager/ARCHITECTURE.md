# FlashManager Architecture

Comprehensive architecture documentation for the STAR Build & Flash System.

## Table of Contents

1. [System Overview](#system-overview)
2. [Component Architecture](#component-architecture)
3. [Data Flow](#data-flow)
4. [Technology Stack](#technology-stack)
5. [Security Architecture](#security-architecture)
6. [Database Schema](#database-schema)
7. [API Design](#api-design)
8. [Deployment Architecture](#deployment-architecture)

## System Overview

FlashManager implements a **poll-based** architecture where local agents poll a remote server for work, enabling remote builds with local hardware flashing across network boundaries.

### Design Goals

1. **Network Flexibility**: Work across different networks without VPN
2. **Firewall Friendly**: No incoming connections to developer machines
3. **Secure**: JWT-based authentication, API key management
4. **Scalable**: Async builds, multiple agents, concurrent operations
5. **Reliable**: Error handling, retry logic, status tracking

### Core Concepts

- **Build Jobs**: Compilation tasks executed on remote server
- **Flash Jobs**: Hardware flashing tasks assigned to local agents
- **Devices**: Registered agents with capabilities
- **Polling**: Agents check every N seconds for pending work
- **Heartbeat**: Agents send regular status updates

## Component Architecture

### 1. Backend (Spring Boot)

```
backend/
├── src/main/kotlin/com/star/flashmanager/
│   ├── config/               # Spring configuration
│   │   ├── SecurityConfig
│   │   ├── GraphQLConfig
│   │   └── JpaAuditingConfig
│   ├── domain/
│   │   ├── entities/         # JPA entities (6)
│   │   └── enums/            # Enums (8)
│   ├── repositories/         # Spring Data JPA (6)
│   ├── services/             # Business logic
│   │   ├── UserService
│   │   ├── AuthService
│   │   ├── DeviceService
│   │   ├── BuildJobService
│   │   ├── FlashJobService
│   │   └── build/            # Build services
│   │       ├── BuildExecutor
│   │       ├── ESP32Builder
│   │       ├── Pi5Builder
│   │       ├── AndroidBuilder
│   │       └── BackendBuilder
│   ├── graphql/
│   │   ├── resolvers/        # GraphQL resolvers (2)
│   │   └── types/            # Input/Output types
│   ├── security/
│   │   ├── JwtTokenProvider
│   │   ├── JwtAuthenticationFilter
│   │   ├── UserPrincipal
│   │   └── DevicePrincipal
│   └── exception/            # Custom exceptions
└── src/main/resources/
    ├── graphql/
    │   └── schema.graphqls   # GraphQL schema
    └── db/migration/         # Flyway migrations
```

**Key Patterns:**
- **Service Layer**: Business logic separation
- **Repository Pattern**: Data access abstraction
- **DTO Pattern**: Input/output types for GraphQL
- **Strategy Pattern**: Multiple builder implementations
- **Async Execution**: Kotlin coroutines for builds

### 2. Frontend (React)

```
frontend/
├── src/
│   ├── contexts/
│   │   └── AuthContext.tsx      # Authentication state
│   ├── graphql/
│   │   └── queries.ts            # All GraphQL operations
│   ├── lib/
│   │   └── apollo-client.ts      # Apollo Client config
│   ├── pages/
│   │   ├── Login.tsx
│   │   ├── Dashboard.tsx
│   │   ├── Builds.tsx
│   │   ├── Devices.tsx
│   │   └── FlashJobs.tsx
│   ├── types/
│   │   └── index.ts              # TypeScript types
│   ├── App.tsx                   # Routing
│   └── main.tsx                  # Entry point
└── e2e/                          # Playwright tests
    ├── auth.spec.ts
    ├── dashboard.spec.ts
    └── builds.spec.ts
```

**Key Patterns:**
- **Context API**: Global auth state
- **Apollo Client**: GraphQL + subscriptions
- **Protected Routes**: Authentication guards
- **Component Composition**: Reusable UI components

### 3. Agent (Rust)

```
agent/
└── src/
    ├── main.rs           # Entry point, polling loop
    ├── client.rs         # GraphQL client
    ├── config.rs         # Configuration
    └── flasher/
        ├── mod.rs        # Flasher trait
        ├── esp32.rs      # ESP32 implementation
        ├── pi5.rs        # Pi5 implementation
        └── android.rs    # Android implementation
```

**Key Patterns:**
- **Trait Pattern**: Flasher abstraction
- **Strategy Pattern**: Multiple flasher implementations
- **Builder Pattern**: Request construction
- **Error Propagation**: Result<T, E> throughout

## Data Flow

### Build Flow

```
User (Frontend)
    │
    ├─ Trigger Build Mutation
    │
    ▼
Backend (GraphQL)
    │
    ├─ MutationResolver.triggerBuild()
    │
    ▼
BuildJobService
    │
    ├─ Create BuildJob (PENDING)
    │
    ▼
BuildExecutor
    │
    ├─ Launch coroutine
    ├─ Update status (RUNNING)
    │
    ▼
Component Builder (ESP32/Pi5/Android/Backend)
    │
    ├─ Checkout branch
    ├─ Execute build command
    ├─ Stream logs to database
    ├─ Copy binary to artifacts
    │
    ▼
BuildExecutor
    │
    ├─ Update status (COMPLETED/FAILED)
    ├─ Store binary path, size, duration
    │
    ▼
Frontend (Subscription/Poll)
    │
    └─ Display updated status
```

### Flash Flow

```
User (Frontend)
    │
    ├─ Create Flash Job Mutation
    │
    ▼
Backend (GraphQL)
    │
    ├─ Validate build is complete
    ├─ Create FlashJob (PENDING)
    │
    ▼
Database
    │
    ▼
Agent (Polling)
    │
    ├─ pollFlashJobs Query
    │
    ▼
Agent receives job
    │
    ├─ claimFlashJob Mutation
    ├─ Update status (RUNNING)
    │
    ▼
Flasher Implementation
    │
    ├─ Download binary
    ├─ Execute flash command
    │   ├─ ESP32: esptool.py
    │   ├─ Pi5: dd
    │   └─ Android: adb
    │
    ▼
Agent reports result
    │
    ├─ completeFlashJob Mutation
    ├─ Update status (COMPLETED/FAILED)
    ├─ Record FlashHistory
    │
    ▼
Frontend (Subscription/Poll)
    │
    └─ Display updated status
```

### Authentication Flow

```
User Enters Credentials
    │
    ▼
Frontend sends Login Mutation
    │
    ▼
Backend AuthService
    │
    ├─ Validate credentials
    ├─ Generate JWT tokens
    │   ├─ Access Token (24h)
    │   └─ Refresh Token (30d)
    │
    ▼
Frontend receives tokens
    │
    ├─ Store in localStorage
    ├─ Set up Apollo Client headers
    │
    ▼
Subsequent Requests
    │
    ├─ Include Bearer token
    │
    ▼
Backend JwtAuthenticationFilter
    │
    ├─ Validate token
    ├─ Set SecurityContext
    │
    ▼
Request processed with authentication
```

### Device Authentication Flow

```
Device Registration (Frontend)
    │
    ├─ User provides name, platform, capabilities
    │
    ▼
Backend generates API key
    │
    ├─ UUID v4
    ├─ Hash with BCrypt
    ├─ Return plain text (one-time only)
    │
    ▼
Agent starts with API key
    │
    ├─ deviceLogin Mutation
    │
    ▼
Backend validates API key
    │
    ├─ Find device by hashed API key
    ├─ Generate device JWT token (30d expiry)
    │
    ▼
Agent receives token
    │
    ├─ Use for all subsequent requests
    ├─ Include in Authorization header
    │
    ▼
Polling and operations authenticated
```

## Technology Stack

### Backend

| Technology | Version | Purpose |
|------------|---------|---------|
| Kotlin | 1.8.22 | Primary language |
| Spring Boot | 3.1.5 | Framework |
| Spring Security | 3.1.5 | Authentication |
| Spring GraphQL | 1.2.3 | GraphQL API |
| PostgreSQL | 15 | Database |
| Flyway | 9.22 | Migrations |
| jjwt | 0.12.3 | JWT tokens |
| Kotlin Coroutines | 1.7.3 | Async execution |
| Gradle | 8.8 | Build tool |

### Frontend

| Technology | Version | Purpose |
|------------|---------|---------|
| React | 18.2 | UI framework |
| TypeScript | 5.2 | Language |
| Vite | 5.0 | Build tool |
| Apollo Client | 3.8 | GraphQL client |
| React Router | 6.20 | Routing |
| Tailwind CSS | 3.3 | Styling |
| Playwright | 1.40 | E2E testing |

### Agent

| Technology | Version | Purpose |
|------------|---------|---------|
| Rust | 1.70+ | Language |
| tokio | 1.35 | Async runtime |
| reqwest | 0.11 | HTTP client |
| serde | 1.0 | Serialization |
| anyhow | 1.0 | Error handling |

## Security Architecture

### Authentication Layers

1. **Web Users**: Email/password → JWT access + refresh tokens
2. **Devices**: API key → Device JWT token
3. **GraphQL**: @PreAuthorize annotations on all endpoints

### Token Management

**Access Token**:
- Expiry: 24 hours
- Claims: userId, email, role
- Used for all API requests

**Refresh Token**:
- Expiry: 30 days
- Claims: userId only
- Used to obtain new access token

**Device Token**:
- Expiry: 30 days
- Claims: deviceId, type=device
- Scope: SCOPE_DEVICE authority

### Authorization

**Roles**:
- `USER`: Standard user, can manage own devices/builds
- `ADMIN`: All permissions, can manage all resources

**Authorities**:
- `ROLE_USER`: Web user access
- `ROLE_ADMIN`: Admin access
- `SCOPE_DEVICE`: Device agent access

**Security Rules**:
```kotlin
// User operations
@PreAuthorize("isAuthenticated()")
fun myDevices(): List<Device>

// Admin operations
@PreAuthorize("hasRole('ADMIN')")
fun devices(): List<Device>

// Device operations
@PreAuthorize("hasAuthority('SCOPE_DEVICE')")
fun pollFlashJobs(): List<FlashJob>
```

## Database Schema

### Entity Relationships

```
User
  ├─ 1:N → Device
  └─ 1:N → BuildJob

Device
  ├─ N:1 → User
  └─ 1:N → FlashJob

BuildJob
  ├─ N:1 → User (triggeredBy)
  ├─ 1:N → BuildLog
  └─ 1:N → FlashJob

FlashJob
  ├─ N:1 → Device
  ├─ N:1 → BuildJob
  └─ 1:1 → FlashHistory

FlashHistory
  └─ 1:1 → FlashJob
```

### Key Tables

**users**
- id (PK)
- email (unique)
- password_hash
- role (enum)
- timestamps

**devices**
- id (PK)
- name
- api_key_hash
- user_id (FK)
- status (enum)
- platform (enum)
- capabilities (array)
- last_seen
- timestamps

**build_jobs**
- id (PK)
- component (enum)
- branch
- status (enum)
- triggered_by (FK → users)
- binary_path
- binary_size
- build_duration
- exit_code
- timestamps

**flash_jobs**
- id (PK)
- build_job_id (FK)
- device_id (FK)
- target_device
- status (enum)
- priority
- claimed_at
- completed_at
- timestamps

**flash_history**
- id (PK)
- flash_job_id (FK, unique)
- result (enum)
- duration
- error_message
- flash_output
- timestamp

### Indexes

- users(email)
- devices(user_id), devices(status)
- build_jobs(status), build_jobs(component), build_jobs(triggered_by)
- flash_jobs(status), flash_jobs(device_id), flash_jobs(build_job_id), flash_jobs(priority)
- flash_history(flash_job_id), flash_history(result)

## API Design

### GraphQL Schema Structure

```graphql
# Scalars
DateTime, Long

# Types (6)
User, Device, BuildJob, BuildLog, FlashJob, FlashHistory

# Enums (8)
UserRole, DeviceStatus, Platform, DeviceCapability,
Component, JobStatus, LogLevel, FlashResult

# Inputs (5)
RegisterInput, LoginInput, RegisterDeviceInput,
TriggerBuildInput, CreateFlashJobInput

# Queries (15)
me, users, user, devices, device, myDevices,
buildJobs, buildJob, buildLogs,
flashJobs, flashJob, flashHistory,
pollFlashJobs

# Mutations (17)
register, login, refreshToken,
registerDevice, updateDeviceStatus, deleteDevice,
triggerBuild, cancelBuild,
createFlashJob, cancelFlashJob,
deviceLogin, claimFlashJob, updateFlashStatus,
completeFlashJob, updateDeviceHeartbeat

# Subscriptions (6)
buildLogStream, flashJobUpdated,
deviceStatusChanged, buildJobUpdated
```

### REST Endpoints

None. All communication through GraphQL.

## Deployment Architecture

### Single Server Deployment

```
┌──────────────────────────────────┐
│         Docker Host              │
│                                  │
│  ┌────────────┐  ┌────────────┐ │
│  │ PostgreSQL │  │  Backend   │ │
│  │  Port 5433 │  │  Port 8081 │ │
│  └────────────┘  └────────────┘ │
│                                  │
│  ┌────────────┐                  │
│  │  Frontend  │                  │
│  │  (Nginx)   │                  │
│  │  Port 80   │                  │
│  └────────────┘                  │
└──────────────────────────────────┘
```

### Coder Workspace Deployment

```
┌──────────────────────────────────┐
│      Kubernetes Cluster          │
│                                  │
│  ┌────────────────────────────┐ │
│  │   FlashManager Workspace   │ │
│  │                            │ │
│  │  Main Container            │ │
│  │  - Backend                 │ │
│  │  - Frontend                │ │
│  │  - Build Tools             │ │
│  │                            │ │
│  │  Sidecar: PostgreSQL       │ │
│  └────────────────────────────┘ │
│                                  │
│  ┌────────────────────────────┐ │
│  │  Persistent Volumes        │ │
│  │  - Workspace               │ │
│  │  - PostgreSQL Data         │ │
│  └────────────────────────────┘ │
└──────────────────────────────────┘
```

### Multi-Region Deployment

```
┌────────── Region 1 ──────────┐
│                               │
│  Backend + Database           │
│  Load Balanced                │
│                               │
└───────────────────────────────┘
         ▲
         │ HTTPS
         │
    ┌────┴────┐
    │         │
┌───▼───┐ ┌──▼────┐
│Agent 1│ │Agent 2│
│Mac    │ │Linux  │
└───────┘ └───────┘
  Local     Local
```

## Performance Considerations

### Backend

- **Build Execution**: Async with coroutines, non-blocking
- **Database**: Connection pooling (HikariCP)
- **GraphQL**: DataLoader pattern (future enhancement)
- **Caching**: In-memory entity caching

### Frontend

- **Apollo Cache**: Normalized cache with cache-first policies
- **Code Splitting**: Route-based lazy loading (future)
- **Bundling**: Vite optimizations, tree-shaking

### Agent

- **Binary Size**: ~2MB release build
- **Memory**: ~10MB typical usage
- **CPU**: <1% idle, varies during flash
- **Network**: Minimal (polls + downloads)

## Scalability

### Horizontal Scaling

- **Backend**: Stateless, can run multiple instances
- **Database**: Single master (can add read replicas)
- **Frontend**: Static files, CDN distribution
- **Agents**: Unlimited, each polls independently

### Vertical Scaling

- **Build Performance**: More CPU/RAM for faster builds
- **Database**: More RAM for caching
- **Storage**: SSD for faster I/O

## Future Enhancements

1. **Real-time Subscriptions**: Fully implement GraphQL subscriptions
2. **Build Log Streaming**: Live log viewer in frontend
3. **Binary Caching**: Avoid re-downloading same binaries
4. **Multi-Device Flashing**: Parallel flashing to multiple devices
5. **Build Queue Management**: Priority-based build scheduling
6. **Metrics & Monitoring**: Prometheus + Grafana
7. **CI/CD Integration**: Webhook triggers for builds
8. **API Rate Limiting**: Protect against abuse
9. **Database Sharding**: Scale beyond single instance
10. **Edge Caching**: CDN for frontend and binaries
