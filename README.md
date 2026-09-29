# Aether64

Aether64 is an OpenXR Android diagnostic/demo application targeting Meta Quest 2, Quest Pro, and Quest 3.

## Quest 2 demo

The Android project builds an arm64 APK for Quest devices. The debug APK can be installed with:

Download the current debug APK directly: [Aether64-Quest2-debug.apk](./Aether64-Quest2-debug.apk)

### Install on Quest 2

Do not open the APK from the Quest Browser. Download it to a computer and install it with SideQuest, or with Android platform-tools after enabling Developer Mode:

```powershell
adb install -r Aether64-Quest2-debug.apk
```

The APK is an arm64 OpenXR Android application. It is not a Windows executable and it is not intended to be launched by tapping the raw download in the browser.

```powershell
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

Build from the repository root with:

```powershell
gradle -p android :app:assembleDebug
```

The GitHub Actions workflow rebuilds the arm64 APK after each push and publishes it as a workflow artifact. The importer accepts a native O2R layout, a ZIP containing a `.z64`, `.n64`, or `.v64` ROM, or an already-unzipped raw ROM file. Importing a ROM does not itself provide the Mario Kart 64 renderer; that adapter still needs to be implemented and linked.

The release build expects signing credentials through the `AETHER_KEYSTORE`, `AETHER_STORE_PASSWORD`, `AETHER_KEY_ALIAS`, and `AETHER_KEY_PASSWORD` environment variables. No signing credentials belong in the repository.
