# Android App

This is the Android application for the Handheld Controller project, built with Kotlin, Jetpack Compose, and Gradle.

## Prerequisites

- Android SDK (automatically managed if using Android Studio)
- Android API Level 23+ (Android 6.0+)

## Building

To build the project, you can use Android Studio or run the following command:

```bash
./gradlew assembleDebug
```

This will generate an APK at `build/outputs/apk/debug/HandheldController-debug.apk`

## Running

### Using Android Studio
- Open the project in Android Studio
- Deploy to a device or emulator

### Manual Installation
- Transfer the generated APK to your Android device
- Install via the Android package installer
- Perfect for Retroid Pocket devices!

## Configuration Notes

- The app is configured for Jetpack Compose UI
- Material Design 3 theming with AppCompat compatibility
- Supports Android 6.0+ (API Level 23+)
- Optimized for handheld gaming devices like Retroid Pocket
