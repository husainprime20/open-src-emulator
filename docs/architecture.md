# OpenSrc runtime and graphics path

## Current Windows-first path

```text
ANDROID GUEST (AOSP 11 x86_64)              WINDOWS HOST
┌───────────────────────────────┐            ┌──────────────────────────┐
│ Android UI / media / apps     │            │ Windows GPU driver       │
│ Skia / guest graphics driver │            │ QEMU SDL presentation    │
│ VirtIO-GPU guest driver      │            │ QEMU VirtIO-GPU device   │
└───────────────┬───────────────┘            └──────────────┬───────────┘
                │ virtio-gpu PCI device                     │
                └───────────────────────────────────────────┘

QEMU guest CPU ───────── WHPX ───────── Windows Hypervisor Platform
```

The current Qt shell starts QEMU with a Q35 machine, a selected VirtIO display device, and a separate QEMU SDL window. It supports a selected x86_64 kernel, optional initrd, system/vendor/userdata disks, USB keyboard/tablet, optional DirectSound audio, and opt-in user-mode NAT. The process is owned by the Qt app; the QEMU monitor on stdio is used for a clean quit. Settings and editable keymap profiles are atomically saved as JSON in `QStandardPaths::AppConfigLocation`.

## Important graphics distinction

The attached reference diagram shows **Linux** `virtio-gpu-pci` + `virglrenderer` + host OpenGL. That Linux rendering stack is not automatically available on Windows. WHPX accelerates guest CPU virtualization; it does not accelerate or implement the GPU path. OpenSrc currently exposes QEMU VirtIO GPU device choices but does not bundle or configure virglrenderer, a Windows VirGL backend, GPU passthrough, or an accelerated Qt guest display. The Windows UI calls the PCI variant experimental and explicitly warns about this boundary.

## Current integration boundaries

- The desktop shell and QEMU process lifecycle are implemented in C++20 / Qt 6.
- The user supplies QEMU and Android-compatible image files. Android images are not included. A generic AOSP `system.img` may not boot unchanged: kernel, ramdisk, partition layout, fstab, kernel drivers, and boot arguments must match the selected QEMU machine/devices. Use a known-good QEMU/ranchu-compatible AOSP 11 x86_64 output set.
- The display currently opens in QEMU's own SDL window, not embedded in the Qt app.
- The keymapping editor imports/exports normalized JSON or simple `action=key` text and persists locally. It does **not** yet inject mapped keys into the guest. Direct MSI App Player / BlueStacks conversion requires real, versioned sample files and is not claimed.
- CPU and RAM allocation, display resolution, WHPX/TCG selection, GPU device, audio, and optional NAT affect the QEMU command. The displayed FPS target was deliberately omitted because this path does not yet implement a frame limiter.
- Monitoring samples host CPU load and the QEMU process working set on Windows. It is not an input-to-photon latency benchmark.

## Privacy / access

No login, cloud sync, or telemetry is implemented. Configuration and keymap files stay under the current Windows user profile's Qt application config directory. Normal Windows file ACLs apply. Android guest privacy and network behavior are separate; NAT networking is off by default.
