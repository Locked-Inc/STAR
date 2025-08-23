# Robot Gateway

This is the robot gateway for the Handheld Controller project, built with Spring Boot, Kotlin, and Gradle. The robot gateway runs on the robot and serves as the communication bridge between the handheld controller and the robot's movement system.

## Prerequisites

- Java 17+
- H2 Database (included) or PostgreSQL (optional for production)

## Building

To build the project, run:

```bash
./gradlew build
```

## Running

To run the application, use:

```bash
./gradlew bootRun
```

The application will be available at `http://localhost:8080`.

## Database Configuration

The robot gateway is configured to use H2 in-memory database for development by default.

For production, you can configure PostgreSQL by:
1. Setting `spring.flyway.enabled=true` in `application.properties`
2. Configuring PostgreSQL connection details
3. Database migrations are handled by Flyway and located in `src/main/resources/db/migration`

## Static Analysis

This project uses [Detekt](https://detekt.dev/) for static analysis.

To run Detekt, use:

```bash
./gradlew detekt
```

A report will be generated in `build/reports/detekt/detekt.html`.
