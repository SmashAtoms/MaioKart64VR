# Aether64 V1

Aether64 V1 is an arm64 OpenXR Android application for Meta Quest 2, Quest Pro, and Quest 3.

## Install

Download the single `Aether64-V1.apk` file and install it with SideQuest or Android platform-tools:

```powershell
adb install -r Aether64-V1.apk
```

Do not try to open an APK directly in Quest Browser.

## Import game data

In the headset, open **Games → Import ZIP or ROM file**. Select either:

- a ZIP containing exactly one `.z64`, `.n64`, or `.v64` ROM; or
- an already-unzipped `.z64`, `.n64`, or `.v64` ROM.

For ZIP input, V1 extracts and validates the ROM before storing it in the app's private directory. The original selected file is not modified.

## Current gameplay status

V1 contains the Quest OpenXR shell, stereo renderer, controller input, ROM importer, and diagnostic scene. It does **not** yet contain an N64 emulator or a linked Mario Kart 64 native engine, so importing a ROM does not currently make the game playable.

The repository's GitHub Actions workflow builds the APK. The Android launcher is Java; the OpenXR renderer and runtime are native C++.
