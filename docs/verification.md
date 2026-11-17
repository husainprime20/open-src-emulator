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

## Verified in CI on Windows (pull_request run 44161601902)

The `Provision real Android runtime (Windows)` job installs into the real
`C:\OpenSrc\Runtime` layout and now passes end to end:

- `sdkmanager` accepted the licences and installed the command-line tools,
  `platform-tools`, the emulator and the 3.5 GB
  `system-images;android-35;google_apis;x86_64` image.
- `avdmanager create avd -n OpenSrc_Default_API_35 -k "system-images;…" -f -d pixel`
  created `avd\OpenSrc_Default_API_35.ini` and `.avd\config.ini`.
- The second (`--twice`) run reported the runtime as already present and did not
  re-download it (resume path).
- The harness exited 0 and the assertion step confirmed the installed files.

Two library behaviours had to be discovered to get there, both now encoded in
`RuntimeInstaller`:

1. `avdmanager` does **not** use JCommander. Options take a separate value
   argument (`-n name`, `-k "sdk_id"`); `--name=value` is rejected by printing
   usage, which is why every earlier attempt failed identically.
2. Only **one** Android preference-root variable may be set. `ANDROID_PREFS_ROOT`
   gets `.android` appended while `ANDROID_USER_HOME` does not, so exporting both
   makes sdklib throw and leaves the SDK handler with a null `androidFolder`;
   `createAvdIniFile` then fails with *"Can't locate Android SDK installation
   directory for the AVD .ini file."*

## Still unverified (must not be claimed as working)

- **Cold boot of the Android guest.** GitHub-hosted Windows runners are
  themselves Hyper-V guests and expose no nested virtualization, so WHPX cannot
  create a partition (`hr=c0350005`) and the x86_64 guest exits immediately.
  The workflow records this and skips the boot check rather than failing. Boot
  must be confirmed on real Windows hardware with virtualization enabled.
- Emulator window behaviour, audio, and networking at runtime.
- The in-app (GUI) download → install flow, which shares this code path but is
  not exercised by CI.
- Keymapper input injection and on-screen overlays (not implemented).
