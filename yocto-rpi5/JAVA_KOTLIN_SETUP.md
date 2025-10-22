# Modern Java and Kotlin Support

## Current Status

The STAR Raspberry Pi 5 image now includes **modern Java and Kotlin support**:

- **OpenJDK 21** (Latest Java LTS release - April 2024)
- **Full Kotlin compatibility** (supports Kotlin 2.0+)
- **ROS2 Jazzy** for robotics integration
- **Python 3.12+** for scripting

## What's Included

### Java Runtime
- **OpenJDK 21 JRE**: Runtime environment for executing Java applications
- **OpenJDK 21 JDK**: Full development kit including compiler (javac)
- **ARM64 optimized**: Native performance on Raspberry Pi 5

### Meta-Java Layer
The build now includes the `meta-java` layer from the Yocto Project:
- Branch: `scarthgap` (Yocto 5.0 LTS)
- Source: https://git.yoctoproject.org/meta-java
- Provides: Modern OpenJDK recipes and Java tooling

## Why OpenJDK 21?

OpenJDK 21 is the latest Long-Term Support (LTS) release:
- **Released**: September 2023
- **Support**: Until September 2031 (8 years)
- **Features**: Virtual threads, pattern matching, records, sealed classes
- **Performance**: Significant improvements over Java 8/11/17
- **Kotlin**: Fully compatible with all modern Kotlin versions (1.9+, 2.0+)

## Kotlin Compatibility

OpenJDK 21 supports all modern Kotlin versions:

| Kotlin Version | Compatibility | Recommended For |
|----------------|---------------|-----------------|
| Kotlin 2.0.x | EXCELLENT | New projects, latest features |
| Kotlin 1.9.x | EXCELLENT | Production, stable |
| Kotlin 1.8.x | GOOD | Legacy projects |
| Kotlin 1.7.x and below | LIMITED | Not recommended |

## Build Configuration

The image is configured with:

### Layers (bblayers.conf)
```
meta-java                      # OpenJDK 21 recipes
meta-oe                        # OpenEmbedded core
meta-python                    # Python support
meta-networking                # Network utilities
meta-raspberrypi              # Raspberry Pi 5 BSP
meta-star                      # Custom STAR configuration
```

### Packages (packagegroup-star-ros2.bb)
```
openjdk-21                    # Java Development Kit
openjdk-21-jre               # Java Runtime Environment
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
# openjdk version "21.0.x" ...
# OpenJDK Runtime Environment ...
# OpenJDK 64-Bit Server VM ...

# Check Java compiler
javac -version
# Expected: javac 21.0.x
```

### Run a Java Application

```bash
# Create a simple test
echo 'public class Test {
    public static void main(String[] args) {
        System.out.println("Java 21 on Raspberry Pi 5!");
        System.out.println("Available processors: " +
            Runtime.getRuntime().availableProcessors());
    }
}' > Test.java

# Compile
javac Test.java

# Run
java Test
```

### Deploy Pre-built JAR Files

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

### Option 1: Install Kotlin on Raspberry Pi (for development)

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

### Option 2: Develop on Host, Deploy to Pi (recommended)

**Best practice for embedded systems:**

1. **On your development machine**, create a Kotlin project:

```bash
# Create new Kotlin application
gradle init --type kotlin-application

# Or use existing project
```

2. **Configure build.gradle.kts** for fat JAR:

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

## Example: Kotlin Robot Controller

Create a simple robot controller:

**RobotController.kt**:
```kotlin
package com.star.robot

import java.net.ServerSocket
import java.io.BufferedReader
import java.io.PrintWriter

class RobotController(private val port: Int = 8080) {
    fun start() {
        println("Starting robot controller on port $port...")
        println("Java version: ${System.getProperty("java.version")}")
        println("Kotlin version: ${KotlinVersion.CURRENT}")

        ServerSocket(port).use { serverSocket ->
            println("Robot controller ready. Waiting for connections...")

            while (true) {
                serverSocket.accept().use { client ->
                    println("Client connected: ${client.inetAddress}")

                    val input = client.getInputStream().bufferedReader()
                    val output = PrintWriter(client.getOutputStream(), true)

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
        }
    }

    private fun handleForward() = "Moving forward"
    private fun handleBackward() = "Moving backward"
    private fun handleLeft() = "Turning left"
    private fun handleRight() = "Turning right"
    private fun handleStop() = "Stopped"
    private fun getStatus() = "Robot OK - Java ${System.getProperty("java.version")}"
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

### Using ROS2 Java Bindings (Advanced)

```kotlin
// Example: Subscribe to ROS2 topic and control robot
import org.ros2.rcljava.RCLJava
import org.ros2.rcljava.node.Node
// ... ROS2 Java bindings

fun main() {
    RCLJava.init()

    val node = RCLJava.createNode("kotlin_robot_controller")

    // Subscribe to movement commands
    // Publish sensor data
    // ... ROS2 integration code
}
```

### Using Process Execution (Simple)

```kotlin
// Call ROS2 CLI tools from Kotlin
import java.lang.ProcessBuilder

fun publishToROS2(topic: String, message: String) {
    ProcessBuilder(
        "bash", "-c",
        "source /opt/ros/jazzy/setup.bash && " +
        "ros2 topic pub $topic std_msgs/msg/String \"data: '$message'\""
    ).start()
}
```

## Performance Considerations

### JVM Tuning for Raspberry Pi 5

```bash
# Limit heap size for embedded system
java -Xmx512m -Xms256m -jar your-app.jar

# Use G1GC for better latency
java -XX:+UseG1GC -Xmx512m -jar your-app.jar

# Enable virtual threads (Java 21 feature)
java -XX:+UnlockExperimentalVMOptions -Xmx512m -jar your-app.jar
```

### Memory Usage

Typical memory footprint:
- **JVM overhead**: 50-100 MB
- **Simple application**: 100-200 MB total
- **Complex application**: 200-500 MB total

Raspberry Pi 5 has 4-8 GB RAM, so this is acceptable.

## Troubleshooting

### Java Not Found

```bash
# Check if Java is installed
which java

# Check package
opkg list-installed | grep openjdk

# If missing, reinstall
opkg update
opkg install openjdk-21
```

### OutOfMemoryError

```bash
# Increase heap size
java -Xmx1g -jar your-app.jar

# Check available memory
free -h
```

### Kotlin Version Mismatch

Make sure your development machine uses a compatible Kotlin version:

```kotlin
// In build.gradle.kts
kotlin("jvm") version "2.0.20"  // Use same version as Pi

// Target JVM 21
kotlin {
    jvmToolchain(21)
}
```

## Build System Integration

The image is built using:

1. **Yocto Scarthgap (5.0 LTS)**
   - Long-term support until 2026
   - Stable, well-tested
   - ARM64 optimized

2. **meta-java Layer**
   - Official Yocto Java support
   - OpenJDK 21 recipes
   - ARM-native compilation

3. **meta-raspberrypi Layer**
   - Raspberry Pi 5 BSP
   - Hardware-specific optimizations
   - Boot firmware

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
│  - Testing                  │
└──────────────┬──────────────┘
               │ scp/rsync
               ▼
┌─────────────────────────────┐
│  Raspberry Pi 5             │
│  (STAR Robot)               │
│                             │
│  - OpenJDK 21               │
│  - ROS2 Jazzy               │
│  - Robot control            │
│  - Sensor integration       │
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
- **Java 21 Features**: https://openjdk.org/projects/jdk/21/
- **ROS2 Documentation**: https://docs.ros.org/en/jazzy/
- **Yocto Project**: https://docs.yoctoproject.org/
- **meta-java Layer**: https://git.yoctoproject.org/meta-java/

## Summary

Your STAR Raspberry Pi 5 image now has:

- **Modern Java**: OpenJDK 21 (LTS, supported until 2031)
- **Full Kotlin Support**: Compatible with Kotlin 2.0+
- **ROS2 Integration**: Jazzy release for robotics
- **Production Ready**: Optimized for embedded ARM64
- **Easy Development**: Build on host, deploy to Pi

Build amazing robotics applications with modern languages!
