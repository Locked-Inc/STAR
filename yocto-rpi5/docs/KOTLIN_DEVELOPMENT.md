# Kotlin Development Guide for STAR Robot

Complete guide for developing Kotlin applications on the Raspberry Pi 5 for robot control, Android remote control integration, and LiDAR data upload.

## Overview

Your STAR robot setup:
- **Robot Controller**: Kotlin app running on Raspberry Pi 5
- **Remote Control**: Android app on Retroid Pocket sending commands to Pi
- **LiDAR Integration**: SICK TIM561 data processing and upload
- **Web Server**: Remote server for LiDAR data upload
- **Communication**: ROS2 for sensor integration, HTTP/WebSocket for Android control

## What's Included

### Java Runtime
- **OpenJDK 8** (1.8.0_272)
- Suitable for Kotlin 1.9+ development
- ARM64 optimized for Raspberry Pi 5

### Kotlin Support
Kotlin will be installed at runtime or deployed with your application JARs.

## Architecture

```
┌─────────────────────┐         ┌──────────────────────┐
│  Retroid Pocket     │         │  Raspberry Pi 5      │
│  (Android)          │         │  (STAR Robot)        │
│                     │         │                      │
│  ┌───────────────┐  │  WiFi/  │  ┌────────────────┐  │
│  │ Controller App│──┼────────>│  │ Kotlin Server  │  │
│  │ (Kotlin/Java) │  │  HTTP/  │  │ App            │  │
│  └───────────────┘  │  WS     │  └────────┬───────┘  │
└─────────────────────┘         │           │          │
                                 │           ▼          │
                                 │  ┌────────────────┐  │
                                 │  │ ROS2 Nodes     │  │
                                 │  │ (LiDAR, etc)   │  │
                                 │  └────────┬───────┘  │
                                 │           │          │
                                 │           ▼          │
                                 │  ┌────────────────┐  │
                                 │  │ Motor Control  │  │
                                 │  └────────────────┘  │
                                 └──────────│───────────┘
                                            │
                                            ▼
                                 ┌──────────────────────┐
                                 │  Remote Web Server   │
                                 │  (LiDAR Data Upload) │
                                 └──────────────────────┘
```

## Setup on Raspberry Pi

### 1. Verify Java Installation

After flashing your image:

```bash
ssh root@192.168.2.100  # Password: star

# Check Java version
java -version
# Should show: openjdk version "1.8.0_272"

# Check Java compiler
javac -version
# Should show: javac 1.8.0_272
```

### 2. Install Kotlin

Since Kotlin isn't available as a Yocto package, install it at runtime:

#### Option A: Use Kotlin Command-Line Compiler

```bash
# Download Kotlin compiler (on the Pi or your dev machine)
cd /opt
wget https://github.com/JetBrains/kotlin/releases/download/v1.9.23/kotlin-compiler-1.9.23.zip
unzip kotlin-compiler-1.9.23.zip
rm kotlin-compiler-1.9.23.zip

# Add to PATH
echo 'export PATH="/opt/kotlinc/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify
kotlinc -version
```

#### Option B: Deploy Kotlin as JAR Dependencies

Build your Kotlin app on your development machine and deploy the compiled JAR with Kotlin runtime bundled:

```kotlin
// build.gradle.kts
tasks.jar {
    manifest {
        attributes["Main-Class"] = "com.star.robot.MainKt"
    }
    from(configurations.runtimeClasspath.get().map { if (it.isDirectory) it else zipTree(it) })
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE
}
```

## Development Workflow

### On Your Development Machine

1. **Setup Kotlin Project**:

```bash
# Create project
mkdir star-robot-controller
cd star-robot-controller

# Initialize Gradle project
gradle init --type kotlin-application
```

2. **Configure build.gradle.kts**:

```kotlin
plugins {
    kotlin("jvm") version "1.9.23"
    application
}

group = "com.star.robot"
version = "1.0.0"

repositories {
    mavenCentral()
}

dependencies {
    // Kotlin standard library
    implementation(kotlin("stdlib"))

    // HTTP server (Ktor)
    implementation("io.ktor:ktor-server-core:2.3.9")
    implementation("io.ktor:ktor-server-netty:2.3.9")
    implementation("io.ktor:ktor-server-websockets:2.3.9")

    // HTTP client (for web server uploads)
    implementation("io.ktor:ktor-client-core:2.3.9")
    implementation("io.ktor:ktor-client-cio:2.3.9")

    // JSON serialization
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.6.3")

    // Logging
    implementation("ch.qos.logback:logback-classic:1.4.14")

    // ROS2 Java bindings (if needed)
    // implementation("org.ros2:rcljava:...")
}

application {
    mainClass.set("com.star.robot.MainKt")
}

tasks.jar {
    manifest {
        attributes["Main-Class"] = "com.star.robot.MainKt"
    }
    from(configurations.runtimeClasspath.get().map {
        if (it.isDirectory) it else zipTree(it)
    })
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE
}
```

### 3. Create Main Application

```kotlin
// src/main/kotlin/com/star/robot/Main.kt
package com.star.robot

import io.ktor.server.application.*
import io.ktor.server.engine.*
import io.ktor.server.netty.*
import io.ktor.server.routing.*
import io.ktor.server.websocket.*
import io.ktor.websocket.*
import kotlinx.coroutines.*
import java.time.Duration

fun main() {
    println("STAR Robot Controller Starting...")

    embeddedServer(Netty, port = 8080, host = "0.0.0.0") {
        install(WebSockets) {
            pingPeriod = Duration.ofSeconds(15)
            timeout = Duration.ofSeconds(15)
            maxFrameSize = Long.MAX_VALUE
            masking = false
        }

        routing {
            get("/") {
                call.respondText("STAR Robot Controller Active")
            }

            // WebSocket for real-time control from Android
            webSocket("/control") {
                try {
                    for (frame in incoming) {
                        frame as? Frame.Text ?: continue
                        val command = frame.readText()
                        handleRobotCommand(command)
                        send("ACK: $command")
                    }
                } catch (e: Exception) {
                    println("WebSocket error: ${e.localizedMessage}")
                }
            }

            // HTTP endpoint for LiDAR data
            post("/lidar/upload") {
                // Handle LiDAR data upload
            }
        }
    }.start(wait = true)
}

suspend fun handleRobotCommand(command: String) {
    println("Received command: $command")
    // Parse command and control robot
    // Integrate with ROS2 nodes for motor control
}
```

### 4. Build and Deploy

```bash
# Build fat JAR
./gradlew jar

# Copy to Pi
scp build/libs/star-robot-controller-1.0.0.jar root@192.168.2.100:/opt/star/
```

### 5. Run on Raspberry Pi

```bash
ssh root@192.168.2.100

# Run the application
java -jar /opt/star/star-robot-controller-1.0.0.jar
```

## Android Remote Control App

### Retroid Pocket Controller

```kotlin
// MainActivity.kt
class MainActivity : AppCompatActivity() {
    private lateinit var webSocket: WebSocket

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        connectToRobot()
        setupControls()
    }

    private fun connectToRobot() {
        val client = OkHttpClient()
        val request = Request.Builder()
            .url("ws://192.168.2.100:8080/control")
            .build()

        webSocket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onMessage(webSocket: WebSocket, text: String) {
                runOnUiThread {
                    updateStatus(text)
                }
            }
        })
    }

    private fun setupControls() {
        // Setup joystick or button controls
        buttonForward.setOnClickListener {
            sendCommand("FORWARD")
        }

        buttonBackward.setOnClickListener {
            sendCommand("BACKWARD")
        }

        // etc...
    }

    private fun sendCommand(command: String) {
        webSocket.send(command)
    }
}
```

## LiDAR Data Processing

### Integrating ROS2 with Kotlin

```kotlin
// LidarProcessor.kt
class LidarProcessor {
    private val client = HttpClient(CIO)
    private val uploadUrl = "https://your-server.com/api/lidar/upload"

    suspend fun processLidarData() {
        // Subscribe to ROS2 LiDAR topic using process execution
        val process = ProcessBuilder(
            "bash", "-c",
            "source /opt/ros/jazzy/setup.bash && ros2 topic echo /scan --once"
        ).start()

        val data = process.inputStream.bufferedReader().readText()
        uploadToServer(data)
    }

    private suspend fun uploadToServer(data: String) {
        try {
            val response = client.post(uploadUrl) {
                setBody(data)
                contentType(ContentType.Application.Json)
            }
            println("Upload status: ${response.status}")
        } catch (e: Exception) {
            println("Upload failed: ${e.message}")
        }
    }
}
```

## SystemD Service Setup

Create a systemd service to auto-start your Kotlin app:

```bash
# /etc/systemd/system/star-robot.service
[Unit]
Description=STAR Robot Controller
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/star
ExecStart=/usr/bin/java -jar /opt/star/star-robot-controller-1.0.0.jar
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
systemctl daemon-reload
systemctl enable star-robot
systemctl start star-robot
systemctl status star-robot
```

## Debugging

### View Logs

```bash
# Application logs
journalctl -u star-robot -f

# Check if service is running
systemctl status star-robot

# Manual run for debugging
java -jar /opt/star/star-robot-controller-1.0.0.jar
```

### Network Testing

```bash
# Test from Android/development machine
curl http://192.168.2.100:8080/

# Test WebSocket
wscat -c ws://192.168.2.100:8080/control
```

## Performance Considerations

### JVM Options for Raspberry Pi 5

```bash
# Optimize for ARM64 and limited memory
java -Xmx512m -XX:+UseG1GC -jar your-app.jar
```

### Tips
1. Use coroutines for async operations
2. Minimize allocations in hot paths
3. Use lazy initialization where possible
4. Profile with VisualVM over SSH

## Upgrading to Newer Java

OpenJDK 8 works but is old. To upgrade to Java 17+ later:

### Option 1: Manual Installation

```bash
# Download Azul Zulu OpenJDK 17 for ARM64
cd /opt
wget https://cdn.azul.com/zulu/bin/zulu17.50.19-ca-jdk17.0.11-linux_aarch64.tar.gz
tar -xzf zulu17.50.19-ca-jdk17.0.11-linux_aarch64.tar.gz

# Update alternatives
update-alternatives --install /usr/bin/java java /opt/zulu17.50.19-ca-jdk17.0.11-linux_aarch64/bin/java 1
update-alternatives --install /usr/bin/javac javac /opt/zulu17.50.19-ca-jdk17.0.11-linux_aarch64/bin/javac 1
```

### Option 2: SDKMAN

```bash
curl -s "https://get.sdkman.io" | bash
source "$HOME/.sdkman/bin/sdkman-init.sh"
sdk install java 17.0.11-zulu
```

## Example Project Structure

```
star-robot-controller/
├── build.gradle.kts
├── settings.gradle.kts
├── src/
│   └── main/
│       └── kotlin/
│           └── com/
│               └── star/
│                   └── robot/
│                       ├── Main.kt
│                       ├── RobotController.kt
│                       ├── LidarProcessor.kt
│                       ├── AndroidBridge.kt
│                       └── WebServerUploader.kt
├── deploy.sh  # Script to build and deploy to Pi
└── README.md
```

## Resources

- [Kotlin Documentation](https://kotlinlang.org/docs/home.html)
- [Ktor Server](https://ktor.io/docs/server.html)
- [ROS2 Documentation](https://docs.ros.org/en/jazzy/)
- [OpenJDK 8 Docs](https://docs.oracle.com/javase/8/docs/)

## Next Steps

1. ✅ Build and flash Yocto image with Java support
2. 📱 Develop Android controller app for Retroid Pocket
3. 🤖 Create Kotlin server app for robot control
4. 🔌 Integrate ROS2 for LiDAR data processing
5. 🌐 Implement web server upload functionality
6. 🧪 Test end-to-end communication
7. 🚀 Deploy and enjoy your robot!
