# Verification log — self-contained C: install + managed Android runtime

Branch: `arena/8c0343cd-open-src-emulator`
Windows Actions run: **44141022504** — ✅ success (all 17 steps)

## What CI actually proved

| Step | Result |
| --- | --- |
| Configure (CMake + Qt 6.8.3 MSVC) | success |
| Build Release (`/W4 /permissive-`, C++20) | success |
| Assemble portable app (`windeployqt`) | success |
| Build installer (`ISCC installer/OpenSrc.iss`) | success |
| Verify installer creates the C-drive runtime layout | success |
| Smoke-test installed app startup | success |
| Upload portable ZIP + installer | success |

The install verification silently installs the produced installer, then asserts:

- `C:\OpenSrc\OpenSrc.exe`
- `C:\OpenSrc\Runtime`
- `C:\OpenSrc\Runtime\sdk`
- `C:\OpenSrc\Runtime\avd`
- `C:\OpenSrc\Runtime\android-user`

and writes/removes a probe file in `Runtime` to prove the folder is user-writable
without elevation. The smoke test launches the installed `OpenSrc.exe` and fails the
build if it exits during startup (missing DLL / bad Qt deployment).

## Bugs found and fixed during verification

1. **`.bat` cannot be started by `QProcess`** — Qt uses `CreateProcess`. `sdkmanager.bat`
   and `avdmanager.bat` now run via `cmd.exe /d /s /c` with per-argument quoting.
2. **Missing archive URL construction** — `error C2065: 'url'`; the relative catalog
   path → `https://dl.google.com/android/repository/...` mapping had been dropped.
3. **`const QDirIterator`** — `error C2662`: `next()` is non-const.
4. **`JAVA_HOME` pointed at `bin`** — must be the JRE root.
5. **Cancelled downloads could resume the pipeline** — reply identity is now checked
   before advancing any stage.
6. **`fail()` could re-enter via the killed child's `finished` signal** — guard added
   so `finished()` is emitted exactly once.
7. **Silent uninstall could hang** on a message box — prompt moved to
   `InitializeUninstall()` and skipped when `UninstallSilent()`.
8. **Invalid workflow YAML** (`C: runtime` in a step name) and a duplicated `shell:` key.

## Upstream facts verified directly

- Adoptium `releases/latest` for `temurin17-binaries` exposes
  `OpenJDK17U-jre_x64_windows_hotspot_*.zip` with a `sha256:` asset digest.
- `repository2-1.xml` embeds the full SDK licence text before the package list, so it
  is large and only a streaming/QXmlStreamReader parse is appropriate.
- `system-images;android-35;google_apis;x86_64` is a real `sdkmanager` package id.

## Still unverified (must not be claimed as working)

- The in-app download → install → **cold boot** of the Android guest on real hardware.
  CI deliberately does not fetch the ~3 GB runtime on every build.
- Whether `pixel_2` is accepted by `avdmanager` as a device id in the shipped
  command-line tools version.
- Emulator window behaviour, audio, and networking at runtime.
- Keymapper input injection and on-screen overlays (not implemented).
