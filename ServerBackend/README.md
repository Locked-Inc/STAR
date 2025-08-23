# Server Backend

This is the server backend for the Handheld Controller project, built with Spring Boot, Kotlin, and Gradle. The server backend runs on a server infrastructure and is responsible for storing and managing robot data including video data, sensor data, position data, and other telemetry information sent from the robot.

## Prerequisites

- Java 17+
- H2 Database (included) or PostgreSQL (recommended for production)

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

The application will be available at `http://localhost:8081`.

## Database Configuration

The server backend is configured to use H2 in-memory database for development by default.

For production, you can configure PostgreSQL by:
1. Setting `spring.flyway.enabled=true` in `application.properties`
2. Configuring PostgreSQL connection details
3. Database migrations are handled by Flyway and located in `src/main/resources/db/migration`

## Data Types

The server backend is designed to handle various types of robot data:

- **Video Data**: Streaming video from robot cameras
- **Sensor Data**: Temperature, humidity, acceleration, gyroscope readings
- **Position Data**: GPS coordinates, indoor positioning data
- **Telemetry**: System status, battery levels, network connectivity
- **Control Logs**: Commands sent to the robot and their execution status

## API Endpoints

The server backend exposes REST APIs for:
- Receiving data from the robot gateway
- Querying historical data
- Real-time data streaming
- Data analytics and reporting

## Static Analysis

This project uses [Detekt](https://detekt.dev/) for static analysis.

To run Detekt, use:

```bash
./gradlew detekt
```

A report will be generated in `build/reports/detekt/detekt.html`.