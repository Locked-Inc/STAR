# Java Support Note

## Current Status

Java (OpenJDK) support has been temporarily removed from the build because:
- The `meta-java` layer is not included in `meta-openembedded` for Yocto Scarthgap (5.0)
- OpenJDK recipes are typically provided by a separate `meta-java` layer
- Adding meta-java requires additional setup and dependencies

## Adding Java Support Later

If you need Java support, you have two options:

### Option 1: Add meta-java Layer (Recommended for Java development)

```bash
cd yocto-rpi5

# Clone meta-java
git clone -b scarthgap https://github.com/pitchumani/meta-java.git

# Add to build/conf/bblayers.conf
# Add this line to BBLAYERS:
#   /home/bsikar/Documents/git/STAR/yocto-rpi5/meta-java \

# Update packagegroup-star-ros2.bb to include:
#   openjdk-21 \
#   openjdk-21-jre \

# Rebuild
bitbake star-minimal-image
```

### Option 2: Install Java at Runtime (Quick and Easy)

After your image is built and running on the Pi:

```bash
# Update package list
opkg update

# Search for Java packages
opkg list | grep openjdk

# Install Java (if available in package feeds)
opkg install openjdk-21

# Or compile from source on the Pi (slow but works)
```

### Option 3: Use a Different Java Implementation

Some alternatives that might be available:
- **Azul Zulu OpenJDK**: ARM builds available
- **Amazon Corretto**: LTS builds for ARM
- **Temurin (Eclipse Adoptium)**: Pre-built ARM binaries

Download and install manually on the Pi after boot.

## Current Build

The current build focuses on **ROS2 Jazzy** which is the primary requirement for:
- SICK TIM561 LiDAR integration
- Robot control and navigation
- Sensor data processing
- SLAM and path planning

Java can be added later if needed for specific applications like:
- ROS2 Java bindings
- Android integration
- JVM-based tools

## Recommendation

**For robotics development**, ROS2 is more critical than Java. Most ROS2 development is done in:
- **Python** (included with ROS2)
- **C++** (included with ROS2)

Java is rarely used in ROS2 ecosystems. If you have a specific need for Java, please specify the use case and we can determine the best approach.

## Quick Start Without Java

The build will now proceed with:
- ✅ ROS2 Jazzy (full installation)
- ✅ Python 3.12+
- ✅ Colcon build tools
- ✅ All ROS2 development tools
- ✅ LiDAR support (sick_scan_xd compatible)
- ❌ Java (can be added later if needed)

Proceed with:
```bash
bitbake star-minimal-image
```
