# OpenSrc

**OpenSrc is a Windows-first Android virtualization desktop app.** The native application is C++20 and Qt 6, with QEMU as the VM process and WHPX as the preferred Windows CPU accelerator. The app is licensed under GPL-3.0-or-later.

This repository now contains a native Qt desktop shell and a QEMU process/configuration layer—not just the earlier browser mock. It can build and launch QEMU using user-selected boot files and settings, and provides local JSON-backed configuration, editable keymap profiles, appearance controls, QEMU logs, and Windows host/QEMU process monitoring.

## Build on Windows

Prerequisites:

- Windows 10/11 x64, Visual Studio 2022 C++ desktop workload
- CMake 3.24 or newer
- Qt 6.5+ (Widgets), built for the same MSVC toolchain
- A Windows QEMU build with the selected machine, WHPX, SDL display, and VirtIO devices enabled
- A QEMU-compatible Android 11 x86_64 image set supplied by you

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build --config Release
```

Use `windeployqt` on `build/Release/OpenSrc.exe` (and ship/install a compatible QEMU build separately) when packaging. The selected QEMU executable and Android image paths are configured in **Android instance** inside the app.

Windows Hypervisor Platform must be enabled in Windows Features for WHPX. TCG is an available fallback, but is much slower. Network is opt-in and uses QEMU user-mode NAT. Android system images are not bundled.

## What works in this first native build

- Qt 6 desktop UI with glass / soft-depth styling, Obsidian / Paper / Aurora themes, a selectable accent, and local-first preferences.
- Local JSON settings for CPU, memory, display size, image paths, WHPX/TCG, GPU device, audio, and network.
- QEMU command construction for Q35, kernel / optional initrd, system / optional vendor and userdata disks, VirtIO display, USB keyboard/tablet, optional audio and NAT.
- Start / stop lifecycle management using `QProcess`; graceful stop through the QEMU monitor before termination fallback.
- QEMU output log and Windows host CPU + QEMU working-set sampling.
- Editable local mapping profile, import/export for normalized JSON and simple `action=key` text.

## Deliberate limitations (not yet equivalent to MSI / BlueStacks)

- Android boot depends on a **compatible** kernel, initrd, partition layout, guest drivers, and boot arguments. A random AOSP `system.img` is not guaranteed to boot. Supply a known-good QEMU/ranchu-compatible AOSP 11 x86_64 image set and validate it on your QEMU build.
- QEMU presents Android in its own SDL window; the guest display is not embedded inside the Qt shell yet.
- Keymap profiles can be edited and persisted, but guest input injection / on-screen control overlays are not connected yet. BlueStacks/MSI proprietary `.cfg` conversion is not claimed; support must be built against real, versioned fixtures.
- The VirtIO GPU device is experimental. The attached VirGL diagram is a **Linux** host path; Windows WHPX is only a CPU virtualization accelerator. This app does not currently wire up virglrenderer or a Windows accelerated VirtIO-GPU renderer. See [graphics architecture](docs/architecture.md).
- Host utilization is basic monitoring, not a low-latency guarantee. No input-to-photon benchmark, FPS cap, GPU passthrough, Android app store, multi-instance manager, or cloud service is implemented yet.

## Configuration and privacy

Settings and keymaps are written as JSON under Qt's `QStandardPaths::AppConfigLocation` in the current user's profile. There is no login, telemetry, or cloud sync. QEMU is launched with a program path and argument vector through `QProcess`, never via a shell command string. Network access is disabled by default. See [docs/architecture.md](docs/architecture.md).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE). OpenSrc is an independent project and is not affiliated with Google, BlueStacks, or MSI.
