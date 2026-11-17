# OpenSrc

OpenSrc is a Windows-first Android emulator application built with C++20 and Qt 6. Its goal is a single install and one control surface — not a requirement to install Android Studio, Android SDK tools, QEMU, or Java, and not a manual image-wiring exercise.

## One installer, one folder, everything on C:

The Windows installer creates a self-contained OpenSrc tree on the system drive:

```text
C:\OpenSrc
├─ OpenSrc.exe, Qt DLLs, licence files     installed by OpenSrc-Setup-x64.exe
└─ Runtime                                created by the installer, writable
   ├─ sdk\                                downloaded Android SDK root
   │  ├─ cmdline-tools\latest\            sdkmanager / avdmanager
   │  ├─ emulator\emulator.exe            Android Emulator
   │  ├─ platform-tools\                  adb and friends
   │  ├─ system-images\…\x86_64\          Android 15 Google APIs image
   │  └─ openjdk\                         private Java runtime (JRE 17)
   ├─ avd\                                the OpenSrc virtual device and its data
   └─ android-user\                       private Android user profile
```

On first launch OpenSrc opens **Android instance** and offers **Install Android runtime**. That single action:

1. downloads the Android command-line tools from Google's SDK repository and verifies the published checksum,
2. downloads an unmodified Eclipse Temurin 17 Windows JRE from the Adoptium releases and verifies its SHA-256,
3. accepts the Android SDK licence **you** approved in the UI and installs the emulator, platform-tools, and the Android 15 x86_64 Google APIs system image,
4. creates the `OpenSrc_Default_API_35` virtual device inside `Runtime\avd`,
5. leaves a ready-to-launch Android device behind — no path picking, no `sdkmanager` by hand, no Android Studio.

Downloads happen in-app with a progress bar, a live log, and a cancel button. Setup is resumable: finished steps are kept, and running it again continues from what is missing.

The default runtime location is `C:\OpenSrc\Runtime`. It can be changed in the app (**Android instance → Change folder…**) or by choosing a different install folder in the installer. OpenSrc only writes inside the folder it owns.

The included system image is **not a Google Play image**. Play Store and Google Play services distribution requires separate authorisation, so OpenSrc does not ship or download them. Apps are installed by APK sideloading.

## Requirements

- Windows 10/11 x64 with hardware virtualisation (SVM/VMX) enabled for acceptable speed.
- Internet access during the first setup, and roughly 3 GB of free space on the target drive.
- The installer can run as a standard user for the default `C:\OpenSrc` location and only requests elevation if you choose a protected folder.

## Build OpenSrc from source

Developer prerequisites: Windows 10/11 x64, Visual Studio 2022 C++ workload, CMake 3.24+, and Qt 6.5+ Widgets and Network for MSVC 2022.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build --config Release
```

## Windows packages

`.github/workflows/windows-build.yml` builds the x64 app, deploys Qt, produces a portable ZIP, compiles the Inno Setup installer, then verifies in CI that the installer creates the `C:\OpenSrc\Runtime` layout, is writable, and that the installed app starts without a missing-DLL failure.

Download **OpenSrc-Windows-Portable-x64** or **OpenSrc-Windows-Installer-x64** from the Actions run's Artifacts section (retained 30 days).

## Current implementation boundaries

- The provisioning pipeline is app-managed and covered by a CI install/startup check, but a full cold boot on a real Windows machine with the downloaded system image has **not** been verified end-to-end in this repository's automation. Treat first-run Android setup as the next thing to validate on hardware.
- Android runs in the emulator's own window. Embedding the device display inside the OpenSrc window, and BlueStacks/MSI-style game control overlays, are not implemented yet.
- Keymap profiles can be edited, imported, and exported locally, but guest input injection and on-screen control overlays are not wired up. Direct BlueStacks/MSI proprietary profile conversion is not claimed.
- No Google Play Store or Play Services. WHPX/Windows Hypervisor Platform is CPU virtualisation, not a custom GPU renderer; Android Emulator manages its own graphics backend.
- The experimental raw-QEMU path still exists for developers, but arbitrary Android images are not promised to boot with it.

OpenSrc has no login, cloud sync, or telemetry. Settings and runtime data stay local. See [runtime architecture](docs/architecture.md), [build/verification log](docs/verification.md), [third-party notices](THIRD-PARTY-NOTICES.md), and [GPL-3.0-or-later](LICENSE).
