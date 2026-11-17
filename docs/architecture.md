# OpenSrc managed Android runtime

## Install layout

The Inno Setup installer places the application on the system drive and creates the runtime subtree next to it:

```text
C:\OpenSrc                       {app}
├─ OpenSrc.exe, Qt DLLs          installed by OpenSrc-Setup-<version>-x64.exe
└─ Runtime                       created with users-modify ACLs
   ├─ sdk\
   │  ├─ cmdline-tools\latest\   downloaded from repository2-1.xml metadata
   │  ├─ emulator\               emulator.exe
   │  ├─ platform-tools\
   │  ├─ system-images\android-35\google_apis\x86_64\
   │  ├─ licenses\               accepted SDK licences
   │  └─ openjdk\                private JRE 17 (Temurin)
   ├─ avd\                       OpenSrc_Default_API_35.ini + .avd\config.ini
   └─ android-user\              ANDROID_USER_HOME / ANDROID_PREFS_ROOT
```

`PrivilegesRequired=lowest` with an explicit `DefaultDirName=C:\OpenSrc` lets a standard user install without a UAC prompt; the installer still elevates if the user selects a protected folder. Because program files keep their default ACLs, only the app-private `Runtime` subtree is made user-writable. That is what allows the multi-gigabyte Android download to happen later inside the app with no second elevation prompt.

The app resolves the same layout at runtime: `AppSettings::defaultAndroidRuntimeRoot()` prefers `<dir of OpenSrc.exe>\Runtime`, verifies it is writable, and otherwise falls back to the user's local application-data folder. A user-chosen folder is persisted as `runtimeRoot` in `settings.json`, and every other Android path (SDK root, emulator executable, AVD home, user home) is derived from it, so the installer, launcher, and readiness checks cannot drift apart.

## Provisioning pipeline

```text
Install Android runtime  (Android instance page)
  ├─ 1  repository2-1.xml → cmdline-tools;latest Windows archive (+SHA-1)
  │      download → verify → tar.exe extract → sdk\cmdline-tools\latest
  ├─ 2  Adoptium GitHub release → Temurin 17 Windows x64 JRE (+SHA-256)
  │      download → verify → tar.exe extract → sdk\openjdk
  ├─ 3  cmd.exe /d /s /c sdkmanager.bat --sdk_root=… --licenses
  │      (answers "y", only after the UI licence checkbox was accepted)
  ├─ 4  cmd.exe /d /s /c sdkmanager.bat --install emulator platform-tools
  │      "system-images;android-35;google_apis;x86_64"
  ├─ 5  cmd.exe /d /s /c avdmanager.bat create avd --name=OpenSrc_Default_API_35
  │      --package … --device-file …   into sdk\..\avd
  └─ 6  ready: emulator.exe + AVD config both present
```

Implementation details that matter:

- **Batch files cannot be started by `CreateProcess`.** Qt's `QProcess` uses `CreateProcess`, so `sdkmanager.bat` is invoked through `cmd.exe /d /s /c` with every argument individually quoted. This is required for installs under paths containing spaces.
- **The repository XML is not fully typed.** `repository2-1.xml` embeds the whole SDK licence text and its `checksum` elements do not reliably carry a `type` attribute, so the digest algorithm is inferred from the value length (40 → SHA-1, 64 → SHA-256). Package channels are read from `channelRef`; the stable channel (`channel-0`) is preferred.
- **Java home must be the JRE root**, not `bin`; `JAVA_HOME`, `ANDROID_SDK_ROOT`, `ANDROID_AVD_HOME`, `ANDROID_USER_HOME`, and `ANDROID_PREFS_ROOT` are all pointed into `Runtime` so nothing leaks into `%USERPROFILE%\.android`.
- **Resume and re-entrancy.** Startup inspects what already exists and skips completed phases. Cancelling or failing clears the active stage before killing the child process, and a guard prevents the killed process from re-entering the completion handler, so `finished()` is emitted exactly once.
- **Progress is monotonic.** Each phase owns a slice of the bar (tools 2–16 %, Java 16–46 %, packages 48–96 %), and `sdkmanager`'s own `NN%` output is mapped into the package slice.

## Launch and process ownership

OpenSrc starts its privately installed `emulator.exe` with `-avd OpenSrc_Default_API_35 -memory <MiB>`, sets the SDK/AVD environment variables described above, and keeps the process handle so it can stop the device cleanly. The Android Emulator opens its own device window.

## CI verification

`.github/workflows/windows-build.yml` builds the MSVC x64 app, deploys Qt, builds the portable ZIP and the Inno Setup installer, then:

1. silently installs the produced installer on the Windows runner,
2. asserts `C:\OpenSrc\OpenSrc.exe` and `Runtime\{sdk,avd,android-user}` exist,
3. writes a probe file into `Runtime` to prove it is writable without elevation,
4. launches the installed `OpenSrc.exe` and fails the build if it exits during startup,
5. removes the test install.

This verifies packaging, install location, folder provisioning, and startup. It does **not** download the multi-gigabyte Android runtime on every CI run, so the end-to-end guest download and cold boot still need validation on a real machine.

## Known boundaries

- Device display is a separate Android Emulator window, not embedded in OpenSrc.
- Keymap profile data is local, but guest key/touch injection and on-screen control overlays are not implemented.
- Android Emulator owns its graphics backend; there is no custom Windows VirGL renderer.
- No Google Play Store or Play Services; the system image supports APK sideloading.
- A failed download leaves partial files in the temporary folder only; the SDK root keeps whatever completed so a retry can resume.
