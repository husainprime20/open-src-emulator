# Third-party notices

## Qt 6.8.3

The Windows workflow dynamically links and deploys Qt 6.8.3 modules (Qt Core, GUI, and Widgets) with OpenSrc. The Qt libraries are distributed under their respective licenses, including the GNU Lesser General Public License version 3 (LGPL-3.0); the applicable license text is included at `licenses/LGPL-3.0.txt`. Qt is not modified by this project. Qt source corresponding to the deployed version is available from the official Qt repositories at <https://code.qt.io/> and the Qt 6.8.3 release/tag sources. Qt license information is available at <https://www.qt.io/licensing/>.

The workflow includes Qt SDK license files in the downloadable package when present in the SDK installation. The packaged Qt DLLs remain replaceable; the installer does not statically link Qt.

## Microsoft Visual C++ runtime

The Windows package uses `windeployqt --compiler-runtime` to deploy the required Microsoft runtime DLLs when applicable. These files remain subject to Microsoft's applicable redistribution terms.

## Runtime components downloaded by OpenSrc

The first-run runtime installer downloads Android SDK command-line tools, Android Emulator, platform-tools, and a Google APIs system image from Google's Android SDK repository. These are not bundled with the OpenSrc installer; the application asks the user to review and accept the Android SDK license before installing them. Android SDK license terms: <https://developer.android.com/studio/terms>.

The installer also downloads an unmodified Eclipse Temurin 17 Windows x64 JRE from the Adoptium GitHub releases so `sdkmanager` works without a system Java installation. Temurin is distributed under GPL-2.0-with-Classpath-Exception; source and notices are published at <https://github.com/adoptium/temurin17-binaries> and <https://adoptium.net/about.html>.

The managed system image is Google APIs, not a Google Play image. OpenSrc does not claim permission to redistribute Google Play Store or Google Play Services.

### Where these files are placed

The Windows installer itself contains only OpenSrc, Qt, and documentation; it does not bundle any Android SDK component or JRE. On first setup OpenSrc downloads the components listed above directly from Google's and Adoptium's distribution services into its own `Runtime` folder and asks the user to accept the Android SDK licence first. Local caching of a downloaded package for the purpose of running OpenSrc locally is not redistribution by OpenSrc; OpenSrc does not host, mirror, or ship those artifacts itself. Anyone repackaging OpenSrc must re-check the upstream terms before bundling them.
