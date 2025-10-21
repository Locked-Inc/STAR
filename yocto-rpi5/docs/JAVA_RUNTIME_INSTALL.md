# Installing Java at Runtime for Kotlin Development

Quick guide to install modern Java (OpenJDK 17 or 21) on your STAR Raspberry Pi 5 after booting the Yocto image.

## Why Runtime Installation?

- **Faster build**: No Java build overhead (saves 2-4 hours)
- **Modern Java**: Get OpenJDK 17 or 21 instead of old OpenJDK 8
- **Better for Kotlin**: Modern Java works better with Kotlin 1.9+
- **Easier updates**: Just download new version when needed

## Prerequisites

Your image includes:
- ✅ `wget` - For downloading
- ✅ `tar` - For extracting
- ✅ `ca-certificates` - For HTTPS downloads

## Option 1: Azul Zulu OpenJDK 21 (Recommended)

Azul provides pre-built ARM64 binaries that work perfectly on Raspberry Pi 5.

### Installation Steps

```bash
# SSH to your Pi
ssh root@192.168.2.100  # Password: star

# Download Azul Zulu JDK 21 for ARM64
cd /opt
wget https://cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-linux_aarch64.tar.gz

# Extract
tar -xzf zulu21.36.17-ca-jdk21.0.4-linux_aarch64.tar.gz

# Create symlink
ln -s zulu21.36.17-ca-jdk21.0.4-linux_aarch64 java

# Add to PATH
echo 'export JAVA_HOME=/opt/java' >> ~/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# Verify installation
java -version
# Should show: openjdk version "21.0.4"

javac -version
# Should show: javac 21.0.4
```

### Make Available to All Users

```bash
# Create system-wide profile
cat > /etc/profile.d/java.sh << 'EOF'
export JAVA_HOME=/opt/java
export PATH=$JAVA_HOME/bin:$PATH
EOF

chmod +x /etc/profile.d/java.sh
```

## Option 2: Adoptium/Temurin OpenJDK 21

Eclipse Adoptium provides another excellent option:

```bash
cd /opt
wget https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.4%2B7/OpenJDK21U-jdk_aarch64_linux_hotspot_21.0.4_7.tar.gz

tar -xzf OpenJDK21U-jdk_aarch64_linux_hotspot_21.0.4_7.tar.gz
ln -s jdk-21.0.4+7 java

echo 'export JAVA_HOME=/opt/java' >> ~/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

java -version
```

## Option 3: OpenJDK 17 (LTS Alternative)

If you prefer Java 17 (also LTS):

```bash
cd /opt
wget https://cdn.azul.com/zulu/bin/zulu17.52.17-ca-jdk17.0.12-linux_aarch64.tar.gz

tar -xzf zulu17.52.17-ca-jdk17.0.12-linux_aarch64.tar.gz
ln -s zulu17.52.17-ca-jdk17.0.12-linux_aarch64 java

echo 'export JAVA_HOME=/opt/java' >> ~/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

## Option 4: SDKMAN (Easiest for Managing Multiple Versions)

SDKMAN lets you install and switch between multiple Java versions easily:

```bash
# Install SDKMAN
curl -s "https://get.sdkman.io" | bash
source "$HOME/.sdkman/bin/sdkman-init.sh"

# Install Java 21
sdk install java 21.0.4-zulu

# Or Java 17
sdk install java 17.0.12-zulu

# List available versions
sdk list java

# Switch versions anytime
sdk use java 21.0.4-zulu
```

## Installing Kotlin

After Java is installed, add Kotlin:

### Method 1: Download Kotlin Compiler

```bash
cd /opt
wget https://github.com/JetBrains/kotlin/releases/download/v1.9.23/kotlin-compiler-1.9.23.zip
unzip kotlin-compiler-1.9.23.zip
rm kotlin-compiler-1.9.23.zip

echo 'export PATH="/opt/kotlinc/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify
kotlinc -version
```

### Method 2: With SDKMAN

```bash
sdk install kotlin
```

## Deploying Your Kotlin Application

You don't need Kotlin on the Pi if you build a fat JAR:

### On Your Development Machine

```kotlin
// build.gradle.kts
tasks.jar {
    manifest {
        attributes["Main-Class"] = "com.star.robot.MainKt"
    }
    // Include all dependencies
    from(configurations.runtimeClasspath.get().map {
        if (it.isDirectory) it else zipTree(it)
    })
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE
}
```

```bash
# Build fat JAR
./gradlew jar

# Deploy to Pi
scp build/libs/star-robot-controller.jar root@192.168.2.100:/opt/star/
```

### On Raspberry Pi

```bash
# Just run with Java
java -jar /opt/star/star-robot-controller.jar
```

## Testing Your Setup

### Create Test Kotlin File

```bash
cd ~
cat > hello.kt << 'EOF'
fun main() {
    println("Hello from Kotlin on STAR Robot!")
    println("Java version: ${System.getProperty("java.version")}")
    println("Kotlin version: ${KotlinVersion.CURRENT}")
}
EOF

# Compile
kotlinc hello.kt -include-runtime -d hello.jar

# Run
java -jar hello.jar
```

## Create SystemD Service for Your App

```bash
# Create service file
cat > /etc/systemd/system/star-robot.service << 'EOF'
[Unit]
Description=STAR Robot Kotlin Controller
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/star
Environment="JAVA_HOME=/opt/java"
Environment="PATH=/opt/java/bin:/usr/bin:/bin"
ExecStart=/opt/java/bin/java -Xmx512m -jar /opt/star/star-robot-controller.jar
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Enable and start
systemctl daemon-reload
systemctl enable star-robot
systemctl start star-robot

# Check status
systemctl status star-robot

# View logs
journalctl -u star-robot -f
```

## Performance Tuning

### JVM Options for Raspberry Pi 5

```bash
# For low memory usage
java -Xmx512m -XX:+UseG1GC -jar your-app.jar

# For better performance
java -Xmx1g -XX:+UseG1GC -XX:MaxGCPauseMillis=200 -jar your-app.jar

# For production
java -server -Xmx1g -XX:+UseG1GC -XX:+UseStringDeduplication -jar your-app.jar
```

## Verification Checklist

After installation, verify:

```bash
# Check Java
java -version
javac -version

# Check environment
echo $JAVA_HOME
echo $PATH

# Test compilation
echo 'public class Test { public static void main(String[] args) { System.out.println("Works!"); } }' > Test.java
javac Test.java
java Test

# Test Kotlin (if installed)
kotlinc -version

# Check memory
free -h
```

## Troubleshooting

### Java Command Not Found

```bash
# Check if Java is installed
ls /opt/java/bin/java

# Re-source bashrc
source ~/.bashrc

# Or add to current session
export JAVA_HOME=/opt/java
export PATH=$JAVA_HOME/bin:$PATH
```

### Permission Denied

```bash
chmod +x /opt/java/bin/*
```

### OutOfMemoryError

```bash
# Increase heap size
java -Xmx1g -jar your-app.jar

# Check available memory
free -h
```

## Recommended Setup

For your Kotlin robot controller use case:

1. **Install Azul Zulu JDK 21** (best ARM64 support)
2. **Build fat JARs** on your dev machine (no need for Kotlin on Pi)
3. **Use SystemD** for auto-start
4. **Set heap to 512MB-1GB** (Pi has 4GB or 8GB)

## Quick Install Script

Save this as `install-java.sh`:

```bash
#!/bin/bash
# Quick Java 21 installer for STAR Robot

cd /opt
wget https://cdn.azul.com/zulu/bin/zulu21.36.17-ca-jdk21.0.4-linux_aarch64.tar.gz
tar -xzf zulu21.36.17-ca-jdk21.0.4-linux_aarch64.tar.gz
rm zulu21.36.17-ca-jdk21.0.4-linux_aarch64.tar.gz
ln -s zulu21.36.17-ca-jdk21.0.4-linux_aarch64 java

cat > /etc/profile.d/java.sh << 'EOF'
export JAVA_HOME=/opt/java
export PATH=$JAVA_HOME/bin:$PATH
EOF

chmod +x /etc/profile.d/java.sh
source /etc/profile.d/java.sh

java -version
echo "Java installed successfully!"
```

Run it:
```bash
chmod +x install-java.sh
./install-java.sh
```

## Next Steps

1. ✅ Flash and boot your Yocto image
2. ✅ SSH to Pi and install Java (5 minutes)
3. ✅ Develop Kotlin app on your dev machine
4. ✅ Build fat JAR
5. ✅ Deploy and run on Pi
6. 🤖 Control your robot from Android!

## Resources

- [Azul Zulu Downloads](https://www.azul.com/downloads/?package=jdk#zulu)
- [Adoptium Temurin](https://adoptium.net/)
- [SDKMAN](https://sdkman.io/)
- [Kotlin Downloads](https://github.com/JetBrains/kotlin/releases)
