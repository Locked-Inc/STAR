# Modern Java and Kotlin Support

## Current Status

The STAR Raspberry Pi 5 image now includes **modern Java and Kotlin support**:

- **OpenJDK 17 JRE** (LTS - supported until September 2029)
- **Full Kotlin compatibility** (supports Kotlin 2.0+, 1.9+, 1.8+)
- **ROS2 Jazzy** for robotics integration
- **Python 3.12+** for scripting
- **Pre-built binaries** from Eclipse Temurin for fast deployment

## What's Included

### Java Runtime
- **OpenJDK 17 JRE**: Java Runtime Environment for executing Java applications
- **Eclipse Temurin binaries**: High-quality, TCK-certified OpenJDK builds
- **ARM64 optimized**: Native performance on Raspberry Pi 5
- **No compilation required**: Uses pre-built binaries for faster image builds

### meta-openjdk-temurin Layer
The build uses the `meta-openjdk-temurin` layer:
- Branch: `scarthgap` (Yocto 5.0)
- Source: https://github.com/lucimber/meta-openjdk-temurin
- Provides: Pre-built Eclipse Temurin JRE binaries
- Available versions: JRE 8, 11, 17, 21
- Default: **OpenJDK 17 JRE** (excellent balance of modern features and stability)

## Why OpenJDK 17?

OpenJDK 17 is a Long-Term Support (LTS) release:
- **Released**: September 2021
- **Support**: Until September 2029 (8+ years)
- **Features**: Records, sealed classes, pattern matching, text blocks, enhanced switch
- **Performance**: Significant improvements over Java 8/11
- **Kotlin**: Fully compatible with all modern Kotlin versions (1.8+, 1.9+, 2.0+)
- **Stability**: Battle-tested, widely adopted in production

## Kotlin Compatibility

OpenJDK 17 supports all modern Kotlin versions:

| Kotlin Version | Compatibility | Recommended For |
|----------------|---------------|-----------------|
| Kotlin 2.0.x | EXCELLENT | New projects, K2 compiler, latest features |
| Kotlin 1.9.x | EXCELLENT | Production, stable, well-tested |
| Kotlin 1.8.x | EXCELLENT | Legacy projects, conservative choice |
| Kotlin 1.7.x and below | GOOD | Older projects (update recommended) |

## Upgrading to OpenJDK 21 (Optional)

If you need Java 21 features, you can upgrade:

### Option 1: Change build configuration

Edit `yocto-rpi5/meta-star/recipes-core/packagegroups/packagegroup-star-ros2.bb`:

```bitbake
# Change from:
openjdk-17-jre \

# To:
openjdk-21-jre \
```

Then rebuild the image.

### Option 2: Install at runtime

After the Pi is running:

```bash
# Using SDKMAN (recommended)
curl -s "https://get.sdkman.io" | bash
source "$HOME/.sdkman/bin/sdkman-init.sh"
sdk install java 21.0.5-tem

# Or download manually
cd /opt
wget https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.5%2B11/OpenJDK21U-jre_aarch64_linux_hotspot_21.0.5_11.tar.gz
tar xzf OpenJDK21U-jre_aarch64_linux_hotspot_21.0.5_11.tar.gz
echo 'export PATH="/opt/jdk-21.0.5+11/bin:$PATH"' >> ~/.bashrc
```

## Build Configuration

The image is configured with:

### Layers (bblayers.conf)
```
meta-openjdk-temurin           # Eclipse Temurin JRE binaries
meta-oe                        # OpenEmbedded core
meta-python                    # Python support
meta-networking                # Network utilities
meta-raspberrypi              # Raspberry Pi 5 BSP
meta-star                      # Custom STAR configuration
```

### Packages (packagegroup-star-ros2.bb)
```
openjdk-17-jre               # Java 17 Runtime Environment
ros-base                      # ROS2 Jazzy
python3                       # Python 3.12+
```

## Using Java on the Raspberry Pi

After flashing and booting the image:

### Verify Java Installation

```bash
ssh root@<pi-ip>

# Check Java version
java -version
# Expected output:
# openjdk version "17.0.x" ...
# OpenJDK Runtime Environment Temurin-17.0.x ...
# OpenJDK 64-Bit Server VM Temurin-17.0.x ...
```

### Run a Java Application

```bash
# Create a simple test
echo 'public class Test {
    public static void main(String[] args) {
        System.out.println("Java 17 on Raspberry Pi 5!");
        System.out.println("Available processors: " +
            Runtime.getRuntime().availableProcessors());

        // Java 17 feature: Text blocks
        var message = """
            Multi-line text blocks
            are awesome in Java 17!
            """;
        System.out.println(message);
    }
}' > Test.java

# Compile (note: JRE doesn't include javac)
# You'll need to compile on your development machine
# or install full JDK separately

# Run pre-compiled class
java Test
```

### Deploy Pre-built JAR Files (Recommended)

The recommended approach for embedded systems:

```bash
# On your development machine (Linux/Mac/Windows):
# 1. Build your Kotlin/Java application
./gradlew shadowJar  # or: ./gradlew build

# 2. Copy to Raspberry Pi
scp build/libs/your-app.jar root@192.168.2.100:/opt/star/

# 3. Run on Pi
ssh root@192.168.2.100 "java -jar /opt/star/your-app.jar"
```

## Kotlin Development

### Option 1: Develop on Host, Deploy to Pi (RECOMMENDED)

**Best practice for embedded systems:**

1. **On your development machine**, create a Kotlin project:

```bash
# Create new Kotlin application
gradle init --type kotlin-application

# Or use existing project
```

2. **Configure build.gradle.kts** for Java 17 and fat JAR:

```kotlin
plugins {
    kotlin("jvm") version "2.0.20"
    id("com.github.johnrengelman.shadow") version "8.1.1"
    application
}

group = "com.star.robot"
version = "1.0.0"

repositories {
    mavenCentral()
}

dependencies {
    implementation(kotlin("stdlib"))
    // Add your dependencies
}

kotlin {
    jvmToolchain(17)  // Target Java 17
}

application {
    mainClass.set("com.star.robot.MainKt")
}

tasks {
    shadowJar {
        archiveBaseName.set("star-robot")
        archiveClassifier.set("")
        archiveVersion.set("")
    }
}
```

3. **Build and deploy**:

```bash
# Build fat JAR (includes all dependencies)
./gradlew shadowJar

# Deploy to Pi
scp build/libs/star-robot.jar root@192.168.2.100:/opt/star/

# Run on Pi
ssh root@192.168.2.100 "java -jar /opt/star/star-robot.jar"
```

### Option 2: Install Kotlin Compiler on Pi (for development/testing)

Using SDKMAN (recommended):
```bash
# On the Raspberry Pi
curl -s "https://get.sdkman.io" | bash
source "$HOME/.sdkman/bin/sdkman-init.sh"

# Install Kotlin
sdk install kotlin

# Verify
kotlin -version
# Expected: Kotlin version 2.x.x
```

Manual installation:
```bash
cd /opt
wget https://github.com/JetBrains/kotlin/releases/download/v2.0.20/kotlin-compiler-2.0.20.zip
unzip kotlin-compiler-2.0.20.zip
echo 'export PATH="/opt/kotlinc/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

## Example: Kotlin Robot Controller

Create a simple robot controller using Java 17 features:

**RobotController.kt**:
```kotlin
package com.star.robot

import java.net.ServerSocket
import java.util.concurrent.Executors

class RobotController(private val port: Int = 8080) {

    fun start() {
        println("Starting robot controller on port $port...")
        println("Java version: ${System.getProperty("java.version")}")
        println("Kotlin version: ${KotlinVersion.CURRENT}")

        // Java 17 feature: Virtual threads (if using Java 21)
        val executor = Executors.newVirtualThreadPerTaskExecutor()

        ServerSocket(port).use { serverSocket ->
            println("Robot controller ready. Waiting for connections...")

            while (true) {
                val client = serverSocket.accept()

                // Handle each client in a separate thread
                executor.submit {
                    handleClient(client)
                }
            }
        }
    }

    private fun handleClient(client: java.net.Socket) {
        client.use {
            println("Client connected: ${client.inetAddress}")

            val input = client.getInputStream().bufferedReader()
            val output = java.io.PrintWriter(client.getOutputStream(), true)

            input.lineSequence().forEach { command ->
                println("Received command: $command")

                val response = when (command.uppercase()) {
                    "FORWARD" -> handleForward()
                    "BACKWARD" -> handleBackward()
                    "LEFT" -> handleLeft()
                    "RIGHT" -> handleRight()
                    "STOP" -> handleStop()
                    "STATUS" -> getStatus()
                    else -> "ERROR: Unknown command"
                }

                output.println(response)
            }
        }
    }

    private fun handleForward() = "Moving forward"
    private fun handleBackward() = "Moving backward"
    private fun handleLeft() = "Turning left"
    private fun handleRight() = "Turning right"
    private fun handleStop() = "Stopped"
    private fun getStatus() = """
        Robot Status:
        - Java: ${System.getProperty("java.version")}
        - Kotlin: ${KotlinVersion.CURRENT}
        - Memory: ${Runtime.getRuntime().freeMemory() / 1024 / 1024} MB free
        - Processors: ${Runtime.getRuntime().availableProcessors()}
        """.trimIndent()
}

fun main() {
    RobotController(port = 8080).start()
}
```

Build and run:
```bash
# Build
./gradlew shadowJar

# Deploy
scp build/libs/robot-controller.jar root@192.168.2.100:/opt/star/

# Run on Pi
ssh root@192.168.2.100 "java -jar /opt/star/robot-controller.jar"
```

Test from another machine:
```bash
# Connect and send commands
telnet 192.168.2.100 8080
> FORWARD
> STATUS
> STOP
```

## Integration with ROS2

You can combine Kotlin/Java with ROS2:

### Using Process Execution (Simple)

```kotlin
// Call ROS2 CLI tools from Kotlin
import java.lang.ProcessBuilder

fun publishToROS2(topic: String, message: String) {
    val command = listOf(
        "bash", "-c",
        "source /opt/ros/jazzy/setup.bash && " +
        "ros2 topic pub $topic std_msgs/msg/String \"data: '$message'\""
    )

    ProcessBuilder(command)
        .redirectOutput(ProcessBuilder.Redirect.INHERIT)
        .redirectError(ProcessBuilder.Redirect.INHERIT)
        .start()
        .waitFor()
}

fun main() {
    publishToROS2("/robot/commands", "FORWARD")
}
```

## Performance Considerations

### JVM Tuning for Raspberry Pi 5

```bash
# Limit heap size for embedded system
java -Xmx512m -Xms256m -jar your-app.jar

# Use G1GC for better latency
java -XX:+UseG1GC -Xmx512m -jar your-app.jar

# Optimize for container/embedded environments
java -XX:+UseContainerSupport -Xmx512m -jar your-app.jar
```

### Memory Usage

Typical memory footprint with OpenJDK 17:
- **JVM overhead**: 50-100 MB
- **Simple application**: 100-200 MB total
- **Complex application**: 200-500 MB total

Raspberry Pi 5 has 4-8 GB RAM, so this is acceptable.

## Troubleshooting

### Java Not Found

```bash
# Check if Java is installed
which java

# Check version
java -version

# If missing, Java wasn't included in build
# Verify packagegroup-star-ros2 includes openjdk-17-jre
```

### OutOfMemoryError

```bash
# Increase heap size
java -Xmx1g -jar your-app.jar

# Check available memory
free -h
```

### Kotlin Version Mismatch

Make sure your development machine targets Java 17:

```kotlin
// In build.gradle.kts
kotlin {
    jvmToolchain(17)  // Must match Pi's Java version
}
```

### No javac Compiler

The image includes **JRE only** (not full JDK):
- **Compile** on your development machine
- **Run** pre-compiled JAR on Pi

To add full JDK (adds ~200MB):
```bash
# At runtime via SDKMAN
sdk install java 17.0.x-tem

# Or rebuild image with full JDK recipe (if available)
```

## Available Java Versions

meta-openjdk-temurin provides multiple Java versions:

| Version | Package Name | LTS | Support Until | Status |
|---------|-------------|-----|---------------|--------|
| Java 8 | openjdk-8-jre | Yes | December 2030 | Legacy |
| Java 11 | openjdk-11-jre | Yes | October 2027 | Stable |
| **Java 17** | **openjdk-17-jre** | **Yes** | **September 2029** | **Default** |
| Java 21 | openjdk-21-jre | Yes | September 2031 | Latest |

**Recommendation**: Stick with Java 17 unless you need specific Java 21 features.

## Development Workflow

Recommended workflow:

```
┌─────────────────────────────┐
│  Development Machine        │
│  (Linux/Mac/Windows)        │
│                             │
│  - IntelliJ IDEA / VS Code  │
│  - Kotlin/Java development  │
│  - Gradle builds            │
│  - Unit testing             │
│  - Java 17 JDK              │
└──────────────┬──────────────┘
               │ scp/rsync
               ▼
┌─────────────────────────────┐
│  Raspberry Pi 5             │
│  (STAR Robot)               │
│                             │
│  - OpenJDK 17 JRE           │
│  - ROS2 Jazzy               │
│  - Robot control            │
│  - Sensor integration       │
│  - Runtime only             │
└─────────────────────────────┘
```

## Next Steps

1. **Flash the image** to SD card
2. **Boot the Raspberry Pi 5**
3. **Verify Java installation**: `java -version`
4. **Deploy your Kotlin application**
5. **Integrate with ROS2** for sensors/actuators
6. **Build your robot!**

## Resources

- **Kotlin Documentation**: https://kotlinlang.org/docs/home.html
- **Java 17 Features**: https://openjdk.org/projects/jdk/17/
- **Eclipse Temurin**: https://adoptium.net/
- **ROS2 Documentation**: https://docs.ros.org/en/jazzy/
- **Yocto Project**: https://docs.yoctoproject.org/
- **meta-openjdk-temurin**: https://github.com/lucimber/meta-openjdk-temurin

## Summary

Your STAR Raspberry Pi 5 image now has:

- **Modern Java**: OpenJDK 17 JRE (LTS, supported until 2029)
- **Full Kotlin Support**: Compatible with Kotlin 2.0+, 1.9+, 1.8+
- **ROS2 Integration**: Jazzy release for robotics
- **Production Ready**: Optimized Eclipse Temurin binaries for ARM64
- **Easy Development**: Build on host, deploy to Pi
- **Upgrade Path**: Can easily upgrade to Java 21 if needed

Build amazing robotics applications with modern languages!
