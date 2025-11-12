# Contributing to FlashManager

Thank you for your interest in contributing to FlashManager! This guide will help you get started with development and contribution guidelines.

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Getting Started](#getting-started)
3. [Development Setup](#development-setup)
4. [Development Workflow](#development-workflow)
5. [Code Standards](#code-standards)
6. [Testing Requirements](#testing-requirements)
7. [Pull Request Process](#pull-request-process)
8. [Project Structure](#project-structure)
9. [Common Tasks](#common-tasks)

## Code of Conduct

This project follows standard open-source conduct principles:

- Be respectful and inclusive
- Welcome newcomers and help them get started
- Focus on constructive feedback
- Respect different viewpoints and experiences
- Accept responsibility for mistakes

## Getting Started

### Prerequisites

Before contributing, ensure you have:

- Java 17+ installed
- Node.js 18+ installed
- Rust 1.70+ installed (for agent development)
- Docker or Colima running (for PostgreSQL)
- Git configured with your name and email

### Fork and Clone

1. **Fork the repository** on GitHub
2. **Clone your fork**:
   ```bash
   git clone https://github.com/YOUR_USERNAME/FlashManager.git
   cd FlashManager
   ```
3. **Add upstream remote**:
   ```bash
   git remote add upstream https://github.com/ORIGINAL_OWNER/FlashManager.git
   ```

## Development Setup

### 1. Backend Setup

```bash
cd backend

# Start PostgreSQL
./docker-start.sh

# Create .env file (if not exists)
cat > .env << EOF
DATABASE_URL=jdbc:postgresql://localhost:5433/flashmanager
DATABASE_USERNAME=flashmanager
DATABASE_PASSWORD=flashmanager123
JWT_SECRET=$(openssl rand -base64 32)
PORT=8081
BUILD_ARTIFACTS_DIR=/workspace/STAR/FlashManager/artifacts
EOF

# Build and run
./gradlew clean build
./gradlew bootRun
```

Backend will be available at `http://localhost:8081/api/v1`

### 2. Frontend Setup

```bash
cd frontend

# Install dependencies
npm install

# Start dev server
npm run dev
```

Frontend will be available at `http://localhost:5174`

### 3. Agent Setup (Optional)

```bash
cd agent

# Build in debug mode (faster compilation)
cargo build

# Or build optimized release
cargo build --release

# Run with debug logging
RUST_LOG=debug cargo run -- \
  --api-key YOUR_API_KEY \
  --server-url http://localhost:8081
```

### 4. Verify Setup

1. Open browser to `http://localhost:5174`
2. Register a new account
3. Register a device to get API key
4. Trigger a test build
5. Verify everything works

## Development Workflow

### Branch Strategy

- **main**: Production-ready code
- **develop**: Development branch (if using Git Flow)
- **feature/**: New features (`feature/device-health-monitoring`)
- **fix/**: Bug fixes (`fix/build-timeout-issue`)
- **docs/**: Documentation updates (`docs/api-examples`)

### Creating a Feature Branch

```bash
# Update your fork
git checkout main
git pull upstream main

# Create feature branch
git checkout -b feature/your-feature-name

# Make your changes
# ...

# Commit changes
git add .
git commit -m "Add feature: your feature description"

# Push to your fork
git push origin feature/your-feature-name
```

### Keeping Your Fork Updated

```bash
# Fetch upstream changes
git fetch upstream

# Merge upstream changes
git checkout main
git merge upstream/main

# Rebase your feature branch
git checkout feature/your-feature-name
git rebase main
```

## Code Standards

### Backend (Kotlin)

**Style Guide:**
- Follow [Kotlin Coding Conventions](https://kotlinlang.org/docs/coding-conventions.html)
- Use detekt for linting: `./gradlew detekt`
- Maximum line length: 120 characters
- Use 4 spaces for indentation

**Key Principles:**
- Prefer immutability (val over var)
- Use data classes for DTOs
- Prefer extension functions over utility classes
- Use sealed classes for type hierarchies
- Leverage Kotlin's null safety

**Example:**
```kotlin
// Good
data class BuildJobInput(
    val component: Component,
    val branch: String = "main"
)

fun BuildJob.toGraphQLType(): BuildJobType = BuildJobType(
    id = this.id,
    component = this.component,
    status = this.status.name
)

// Avoid
class BuildJobInput {
    var component: Component? = null
    var branch: String? = null
}
```

**Run detekt before committing:**
```bash
./gradlew detekt
```

### Frontend (TypeScript/React)

**Style Guide:**
- Follow [Airbnb React/JSX Style Guide](https://github.com/airbnb/javascript/tree/master/react)
- Use ESLint: `npm run lint`
- Use Prettier for formatting
- Maximum line length: 100 characters

**Key Principles:**
- Functional components with hooks
- TypeScript strict mode
- Props interfaces for all components
- Custom hooks for reusable logic
- Context for global state

**Example:**
```typescript
// Good
interface BuildListProps {
  projectId: string;
  onBuildClick?: (buildId: string) => void;
}

export const BuildList: React.FC<BuildListProps> = ({ projectId, onBuildClick }) => {
  const { data, loading, error } = useQuery(BUILD_JOBS_QUERY, {
    variables: { projectId }
  });

  if (loading) return <Spinner />;
  if (error) return <ErrorMessage error={error} />;

  return (
    <div className="build-list">
      {data.buildJobs.map(build => (
        <BuildCard key={build.id} build={build} onClick={onBuildClick} />
      ))}
    </div>
  );
};

// Avoid - class components, any types
```

**Run linting before committing:**
```bash
npm run lint
npm run lint:fix  # Auto-fix issues
```

### Agent (Rust)

**Style Guide:**
- Follow [Rust Style Guide](https://doc.rust-lang.org/1.0.0/style/)
- Use rustfmt: `cargo fmt`
- Use clippy: `cargo clippy`

**Key Principles:**
- Prefer ? operator over unwrap()
- Use Result<T, E> for error handling
- Implement traits for abstractions
- Use anyhow for error types
- Document public APIs

**Example:**
```rust
// Good
pub fn flash(&self, binary_path: &str, target: &str) -> Result<FlashResult> {
    let output = Command::new("esptool.py")
        .args(["--port", target, "write_flash", "0x0", binary_path])
        .output()
        .context("Failed to execute esptool")?;

    if output.status.success() {
        Ok(FlashResult::Success)
    } else {
        Err(anyhow!("Flash failed: {}", String::from_utf8_lossy(&output.stderr)))
    }
}

// Avoid - unwrap(), panic!
pub fn flash(&self, binary_path: &str, target: &str) -> FlashResult {
    let output = Command::new("esptool.py")
        .args(["--port", target, "write_flash", "0x0", binary_path])
        .output()
        .unwrap();  // BAD!

    FlashResult::Success
}
```

**Run checks before committing:**
```bash
cargo fmt --check
cargo clippy -- -D warnings
```

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

**Examples:**
```
feat(backend): add device heartbeat tracking

Implement periodic heartbeat updates from agents to track device online status.
Devices that haven't sent heartbeat in 60 seconds are marked as OFFLINE.

Closes #42

---

fix(frontend): correct GraphQL subscription reconnection

WebSocket connection now properly reconnects after network interruptions
using Apollo Client's reconnection logic.

Fixes #38

---

docs(readme): update deployment instructions

Add Docker Compose deployment section with complete example configuration.
```

## Testing Requirements

### Backend Tests

All backend changes must include tests:

```bash
# Run all tests
./gradlew test

# Run specific test
./gradlew test --tests BuildJobServiceTest

# Run with coverage
./gradlew jacocoTestReport
```

**Required test types:**
- **Unit tests**: Service logic, utilities
- **Integration tests**: Repository queries, GraphQL resolvers
- **Security tests**: Authentication, authorization

**Example test:**
```kotlin
@SpringBootTest
class BuildJobServiceTest {
    @Autowired
    private lateinit var buildJobService: BuildJobService

    @MockBean
    private lateinit var buildExecutor: BuildExecutor

    @Test
    fun `triggerBuild should create pending build job`() {
        val result = buildJobService.triggerBuild(
            component = Component.ESP32_FIRMWARE,
            branch = "main",
            triggeredBy = "user123"
        )

        assertThat(result.status).isEqualTo(JobStatus.PENDING)
        assertThat(result.component).isEqualTo(Component.ESP32_FIRMWARE)
        verify(buildExecutor).executeBuild(any())
    }
}
```

### Frontend Tests

Frontend changes should include tests where applicable:

```bash
# Run unit tests
npm run test

# Run with coverage
npm run test:coverage

# Run E2E tests
npm run test:e2e
```

**Test types:**
- **Component tests**: UI rendering, interactions
- **Hook tests**: Custom hook logic
- **E2E tests**: Critical user flows

**Example test:**
```typescript
import { render, screen, fireEvent } from '@testing-library/react';
import { BuildList } from './BuildList';

describe('BuildList', () => {
  it('displays builds from query', async () => {
    const mockBuilds = [
      { id: '1', component: 'ESP32', status: 'COMPLETED' }
    ];

    render(
      <MockedProvider mocks={[/* Apollo mocks */]}>
        <BuildList projectId="123" />
      </MockedProvider>
    );

    expect(await screen.findByText('ESP32')).toBeInTheDocument();
    expect(screen.getByText('COMPLETED')).toBeInTheDocument();
  });
});
```

### Agent Tests

```bash
# Run tests
cargo test

# Run specific test
cargo test test_esp32_flash

# Run with output
cargo test -- --nocapture
```

## Pull Request Process

### Before Submitting

1. **Update from upstream**:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Run all checks**:
   ```bash
   # Backend
   cd backend
   ./gradlew clean build test detekt

   # Frontend
   cd frontend
   npm run lint
   npm run test
   npm run build

   # Agent
   cd agent
   cargo fmt --check
   cargo clippy
   cargo test
   ```

3. **Update documentation** if needed:
   - Update README.md for user-facing changes
   - Update ARCHITECTURE.md for architectural changes
   - Add code comments for complex logic

### Submitting Pull Request

1. **Push your branch**:
   ```bash
   git push origin feature/your-feature-name
   ```

2. **Create PR on GitHub**:
   - Use descriptive title following commit conventions
   - Fill out PR template completely
   - Link related issues
   - Add screenshots for UI changes
   - List any breaking changes

3. **PR Description Template**:
   ```markdown
   ## Description
   Brief description of changes.

   ## Type of Change
   - [ ] Bug fix
   - [ ] New feature
   - [ ] Breaking change
   - [ ] Documentation update

   ## Testing
   - [ ] Unit tests pass
   - [ ] Integration tests pass
   - [ ] E2E tests pass (if applicable)
   - [ ] Manual testing completed

   ## Checklist
   - [ ] Code follows project style guidelines
   - [ ] Self-review completed
   - [ ] Comments added for complex code
   - [ ] Documentation updated
   - [ ] No new warnings generated
   - [ ] Tests added/updated

   ## Related Issues
   Closes #42
   Related to #38

   ## Screenshots (if applicable)
   ```

### Code Review

- Address all review comments
- Keep discussions focused and professional
- Make requested changes in new commits
- Squash commits before merge (if requested)

### After Merge

1. **Delete your feature branch**:
   ```bash
   git branch -d feature/your-feature-name
   git push origin --delete feature/your-feature-name
   ```

2. **Update your main branch**:
   ```bash
   git checkout main
   git pull upstream main
   ```

## Project Structure

Understanding the codebase structure:

### Backend
```
backend/src/main/kotlin/com/star/flashmanager/
├── config/           # Spring configuration
├── controllers/      # REST controllers
├── domain/
│   ├── entities/    # JPA entities
│   └── enums/       # Enums
├── dto/             # Data transfer objects
├── graphql/
│   ├── resolvers/   # GraphQL resolvers
│   └── types/       # GraphQL types
├── repositories/    # Spring Data repositories
├── services/        # Business logic
│   └── build/       # Build system
├── security/        # Authentication/authorization
└── exception/       # Custom exceptions
```

### Frontend
```
frontend/src/
├── components/      # React components
├── pages/           # Page components
├── graphql/         # GraphQL operations
├── services/        # Apollo Client setup
├── hooks/           # Custom hooks
├── types/           # TypeScript types
└── utils/           # Utilities
```

### Agent
```
agent/src/
├── main.rs          # Entry point
├── client.rs        # GraphQL client
├── config.rs        # Configuration
└── flasher/         # Flash implementations
    ├── mod.rs       # Trait definition
    ├── esp32.rs     # ESP32 flasher
    ├── pi5.rs       # Pi5 flasher
    └── android.rs   # Android flasher
```

## Common Tasks

### Adding a New Build Component

1. **Add enum value** in `backend/.../domain/enums/Component.kt`:
   ```kotlin
   enum class Component {
       ESP32_FIRMWARE,
       PI5_OS,
       ANDROID_APP,
       ROBOT_GATEWAY,
       SERVER_BACKEND,
       YOUR_NEW_COMPONENT  // Add here
   }
   ```

2. **Create builder** in `backend/.../services/build/`:
   ```kotlin
   @Service
   class YourNewComponentBuilder(
       @Value("\${flashmanager.build.your-component.repo-path}")
       private val repoPath: String,
       // ...
   ) {
       fun build(buildJob: BuildJob): BuildExecutor.BuildResult {
           // Implementation
       }
   }
   ```

3. **Update BuildExecutor** to route builds:
   ```kotlin
   val result = when (buildJob.component) {
       // ... existing cases
       Component.YOUR_NEW_COMPONENT -> yourNewComponentBuilder.build(buildJob)
   }
   ```

4. **Add configuration** in `application.yml`:
   ```yaml
   flashmanager:
     build:
       your-component:
         repo-path: ${YOUR_COMPONENT_REPO_PATH:/path/to/repo}
   ```

5. **Update frontend** component selector in `Builds.tsx`

6. **Write tests** for the new builder

### Adding a New GraphQL Query

1. **Update schema** in `schema.graphqls`:
   ```graphql
   type Query {
       yourNewQuery(input: YourInput!): YourOutput!
   }
   ```

2. **Add resolver method** in `QueryResolver.kt`:
   ```kotlin
   @QueryMapping
   fun yourNewQuery(@Argument input: YourInput): YourOutput {
       return yourService.doSomething(input)
   }
   ```

3. **Add frontend query** in `frontend/src/graphql/queries.ts`:
   ```typescript
   export const YOUR_NEW_QUERY = gql`
     query YourNewQuery($input: YourInput!) {
       yourNewQuery(input: $input) {
         field1
         field2
       }
     }
   `;
   ```

4. **Use in component**:
   ```typescript
   const { data, loading, error } = useQuery(YOUR_NEW_QUERY, {
     variables: { input: { /* ... */ } }
   });
   ```

### Adding a Database Migration

1. **Create migration file** in `backend/src/main/resources/db/migration/`:
   ```sql
   -- V4__add_your_new_table.sql
   CREATE TABLE your_new_table (
       id VARCHAR(255) PRIMARY KEY,
       name VARCHAR(255) NOT NULL,
       created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
   );

   CREATE INDEX idx_your_new_table_name ON your_new_table(name);
   ```

2. **Create/update entity** in `backend/.../domain/entities/`:
   ```kotlin
   @Entity
   @Table(name = "your_new_table")
   data class YourNewEntity(
       @Id
       val id: String = UUID.randomUUID().toString(),

       @Column(nullable = false)
       val name: String,

       @Column(name = "created_at", nullable = false)
       val createdAt: OffsetDateTime = OffsetDateTime.now()
   )
   ```

3. **Restart backend** - Flyway auto-applies migration

4. **Verify migration**:
   ```bash
   psql -U flashmanager -d flashmanager -c '\dt'
   ```

## Getting Help

- **Issues**: Check existing issues or create a new one
- **Discussions**: Use GitHub Discussions for questions
- **Documentation**: Refer to README.md, ARCHITECTURE.md, DEPLOYMENT.md

Thank you for contributing to FlashManager!
