# OpenSrc

OpenSrc is a Windows-first desktop front end for running Android Virtual Devices with the **Android Emulator SDK**. The native UI is C++20 / Qt 6. On the supported path, OpenSrc launches Google's Android Emulator with an AVD; that runtime owns the matched Android kernel, ramdisk, system/vendor images, virtual hardware, and boot parameters. Android Emulator selects an available supported Windows hypervisor accelerator (such as WHPX) when configured.

OpenSrc also retains an advanced **raw QEMU** path for compatible custom images. It is experimental and is not the recommended way to boot Android Studio SDK images. OpenSrc does not implement a new Android kernel or graphics stack from scratch.

## Get Android running

1. Install [Android Studio](https://developer.android.com/studio).
2. In **SDK Manager → SDK Tools**, install **Android Emulator**. In **SDK Platforms** (or Device Manager), install an x86_64 system image, such as Android 11 / API 30.
3. In **Tools → Device Manager**, create an AVD using that image and boot it once from Android Studio to confirm the SDK setup.
4. Open OpenSrc → **Android instance**. The app attempts to find `emulator.exe` in `ANDROID_SDK_ROOT`, `ANDROID_HOME`, or the default Windows SDK directory. If it is not found, browse to `<Android SDK>\emulator\emulator.exe`—**not** `qemu-w64-setup-....exe`.
5. Click **Refresh AVDs**, select the device, choose a memory override, save, then launch. The AVD keeps its own image files and persistent user data.

You do **not** need to select a separate kernel, ramdisk, or system image in Android Emulator SDK mode. The Android Emulator loads those as a matched AVD. The SDK image can be downloaded from Android Studio under its own license.

## Raw QEMU mode (experimental)

This mode is only for Android image sets built and configured specifically for the selected QEMU version, machine, device models, and boot arguments. A standard Android Studio `system.img` plus a separately downloaded kernel is not guaranteed to boot with generic QEMU flags. In Raw QEMU mode select the actual `qemu-system-x86_64.exe` executable, never the QEMU installer, then supply a complete compatible image set. Use the SDK mode if your goal is simply to run an Android Studio AVD.

## Build OpenSrc on Windows

Prerequisites: Windows 10/11 x64, Visual Studio 2022 C++ workload, CMake 3.24+, and Qt 6.5+ Widgets for MSVC 2022.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build --config Release
```

## Download builds from GitHub Actions

`.github/workflows/windows-build.yml` builds the x64 app, deploys Qt, creates a portable ZIP, and compiles an Inno Setup installer on pushes, pull requests, and manual runs. Download **OpenSrc-Windows-Portable-x64** or **OpenSrc-Windows-Installer-x64** from the Actions run’s Artifacts section. Artifacts are retained for 30 days. A `v*.*.*` tag publishes both packages on a GitHub Release. Neither package bundles Android Studio, QEMU, Android images, or AVDs.

## Features and honest boundaries

- Obsidian / Paper / Aurora themes, local JSON settings, local keymap profile editing/import/export, process logs, and Windows CPU/process-memory monitoring.
- SDK mode launches and lists existing AVDs and lets you override RAM. Android Studio / Device Manager owns AVD creation and its CPU, display, GPU, audio, and network profile.
- Raw QEMU mode has configurable Q35, WHPX/TCG, VirtIO, disk, USB, audio, and NAT arguments, but generic Android image boot is not validated.
- Keymap profiles currently save and import/export; guest key injection and on-screen mappings are not wired up yet. Direct MSI / BlueStacks config conversion is not claimed.
- The Windows VirGL path shown in the supplied diagram is not implemented. WHPX accelerates CPU virtualization; it is not a GPU renderer.

OpenSrc has no login, cloud sync, or telemetry. App configuration is stored under the current Windows user's Qt application-data directory. See [graphics/runtime architecture](docs/architecture.md), [third-party notices](THIRD-PARTY-NOTICES.md), and [GPL-3.0-or-later](LICENSE).
