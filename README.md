# Aether64

Aether64 is an OpenXR Android diagnostic/demo application targeting Meta Quest 2, Quest Pro, and Quest 3.

## Quest 2 demo

The Android project builds an arm64 APK for Quest devices. The debug APK can be installed with:

Download the current debug APK directly: [Aether64-Quest2-debug.apk](./Aether64-Quest2-debug.apk)

```powershell
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

Build from the repository root with:

```powershell
gradlew.bat -p android :app:assembleDebug
```

The release build expects signing credentials through the `AETHER_KEYSTORE`, `AETHER_STORE_PASSWORD`, `AETHER_KEY_ALIAS`, and `AETHER_KEY_PASSWORD` environment variables. No signing credentials belong in the repository.
