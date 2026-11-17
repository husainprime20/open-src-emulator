# Third-party notices

## Qt 6.8.3

The Windows workflow dynamically links and deploys Qt 6.8.3 modules (Qt Core, GUI, and Widgets) with OpenSrc. The Qt libraries are distributed under their respective licenses, including the GNU Lesser General Public License version 3 (LGPL-3.0); the applicable license text is included at `licenses/LGPL-3.0.txt`. Qt is not modified by this project. Qt source corresponding to the deployed version is available from the official Qt repositories at <https://code.qt.io/> and the Qt 6.8.3 release/tag sources. Qt license information is available at <https://www.qt.io/licensing/>.

The workflow includes Qt SDK license files in the downloadable package when present in the SDK installation. The packaged Qt DLLs remain replaceable; the installer does not statically link Qt.

## Microsoft Visual C++ runtime

The Windows package uses `windeployqt --compiler-runtime` to deploy the required Microsoft runtime DLLs when applicable. These files remain subject to Microsoft's applicable redistribution terms.
