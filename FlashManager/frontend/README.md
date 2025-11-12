# FlashManager Frontend

React-based frontend for the FlashManager STAR Build & Flash System.

## Tech Stack

- **React 18.2** with TypeScript
- **Vite 5.0** - Build tool and dev server
- **Apollo Client 3.8** - GraphQL client with subscriptions
- **React Router 6.20** - Client-side routing
- **Tailwind CSS 3.3** - Utility-first CSS framework
- **Playwright 1.40** - End-to-end testing

## Prerequisites

- Node.js 18+ and npm
- Backend server running on http://localhost:8081

## Getting Started

### 1. Install Dependencies

```bash
npm install
```

### 2. Start Development Server

```bash
npm run dev
```

The app will be available at http://localhost:5174

### 3. Build for Production

```bash
npm run build
```

### 4. Preview Production Build

```bash
npm run preview
```

## Testing

### Unit Tests (Vitest)

```bash
npm run test
```

### End-to-End Tests (Playwright)

**Note**: Backend server must be running for E2E tests.

```bash
# Run E2E tests (headless)
npm run test:e2e

# Run E2E tests with UI
npm run test:e2e:ui

# Run E2E tests in debug mode
npm run test:e2e:debug
```

### E2E Test Structure

- `e2e/auth.spec.ts` - Authentication flows (login, register)
- `e2e/dashboard.spec.ts` - Dashboard and navigation
- `e2e/builds.spec.ts` - Build management and triggering

## Features

### Authentication
- User registration
- User login
- JWT token management
- Protected routes

### Dashboard
- Real-time statistics
- Active builds count
- Online devices count
- Pending flash jobs count
- Recent builds and devices lists

### Builds Page
- View all build jobs
- Trigger new builds
- Select component (ESP32, Pi5, Android, Backends)
- Select branch
- Real-time status updates

### Devices Page
- View all registered devices
- Device status (Online, Offline, Busy)
- Platform information
- Capabilities

### Flash Jobs Page
- View all flash jobs
- Job status tracking
- Device and component information

## GraphQL Integration

The app uses Apollo Client with:
- HTTP link for queries and mutations
- WebSocket link for subscriptions
- Automatic token injection
- Cache management

### GraphQL Endpoints

- HTTP: `http://localhost:8081/api/v1/graphql`
- WebSocket: `ws://localhost:8081/api/v1/graphql`

## Code Structure

```
src/
├── contexts/          # React contexts (Auth)
├── graphql/           # GraphQL queries, mutations, subscriptions
├── lib/              # Apollo Client configuration
├── pages/            # Page components (Login, Dashboard, etc.)
├── types/            # TypeScript type definitions
├── App.tsx           # Main app with routing
├── main.tsx          # Entry point
└── index.css         # Global styles

e2e/                  # Playwright E2E tests
```

## Environment Variables

The app connects to the backend at:
- GraphQL HTTP: `http://localhost:8081/api/v1/graphql`
- GraphQL WebSocket: `ws://localhost:8081/api/v1/graphql`

These are hardcoded in `src/lib/apollo-client.ts`. For production, use environment variables.

## Development Workflow

1. Start backend: `cd ../backend && ./gradlew bootRun`
2. Start frontend: `npm run dev`
3. Open browser: http://localhost:5174
4. Register a new account
5. Navigate through the app

## Testing Workflow

1. Ensure backend is running
2. Run E2E tests: `npm run test:e2e`
3. Tests will automatically:
   - Register new users
   - Login
   - Navigate pages
   - Trigger builds
   - Verify data

## Known Limitations

- No real-time subscription updates yet (basic polling)
- No build log streaming viewer
- No device registration UI
- No flash job creation UI
- Limited error handling UI

## Future Enhancements

- Real-time build log streaming with subscriptions
- Device registration flow
- Flash job creation from builds page
- Build artifacts download
- Flash history visualization
- Admin user management
