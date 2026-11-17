# OpenSrc runtime and graphics paths

## Recommended: Android Emulator SDK / AVD

```text
OpenSrc Qt desktop UI
  └─ launches emulator.exe -avd <name> -memory <MiB>
       └─ Android Emulator's QEMU-derived engine + supported Windows acceleration
            ├─ loads the AVD-matched kernel-ranchu and ramdisk
            ├─ loads system/vendor/userdata and AVD hardware config
            └─ opens its Android device window
```

This is the supported way in this project to run Android Studio system images: the Android SDK Emulator interprets the AVD metadata and assembles the right boot arguments/devices. OpenSrc discovers AVD names with `emulator.exe -list-avds`, stores the chosen AVD and emulator path in local JSON, and starts it as a child process. AVD creation and installation of the system image are handled by Android Studio's SDK Manager and Device Manager. CPU, screen, GPU, audio, and networking settings belong to the AVD profile; OpenSrc currently passes a memory override.

## Advanced: raw QEMU prototype

```text
Android guest (user-supplied compatible image set)
  -> QEMU Q35 + WHPX/TCG + VirtIO devices
  -> QEMU SDL window + Windows display stack
```

The raw mode constructs a QEMU argument vector from the user-selected kernel/initrd/system/vendor/userdata files and virtual hardware choices. It is retained for experimentation, not advertised as a universal Android boot path. Android boot requires a matched kernel, ramdisk, partitions/fstab, guest drivers, machine, and boot args; random SDK images are not guaranteed to boot through this generic path.

## Graphics distinction

The supplied reference diagram's `virtio-gpu-pci -> virglrenderer -> host OpenGL` route is a **Linux** host path. Windows WHPX is a CPU accelerator, not a GPU renderer. OpenSrc does not currently provide Windows VirGL, GPU passthrough, or an integrated Qt guest display. In SDK mode Android Emulator owns graphics rendering and displays the device in its own window.

## Current boundaries

- No Android system image is bundled. Google SDK images are acquired and accepted under Android Studio / SDK licensing.
- Keymap profiles persist locally and import/export normalized JSON or simple `action=key` text, but input remapping is not injected into the guest yet. No direct BlueStacks/MSI `.cfg` conversion is claimed.
- Performance monitoring reports host CPU and emulator-process working set; it is not a latency benchmark.
- Settings and keymaps live in Qt `QStandardPaths::AppConfigLocation`; no account, telemetry, or cloud sync is implemented.
