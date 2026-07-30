# better Model Viewer

A Qt front-end for [WoW Model Viewer](https://github.com/wowmodelviewer/wowmodelviewer),
replacing the wxWidgets one. The model, render and data layers are not touched:
`core.dll` and `wow.dll` are linked and used exactly as upstream builds them. Only the
GUI is new.

Released as **0.1.0-beta** — several areas of the old interface are not ported yet.
See [LIESMICH.txt](installer/LIESMICH.txt) for the current feature list, and
[MIGRATION.md](MIGRATION.md) for why the port is shaped the way it is.

## Why this exists

The measurement that started it: of the four modules in the upstream tree, three
(`core`, `games/wow`, `plugins`) contain zero wxWidgets includes. All 19,700 lines of
toolkit code sit in the executable alone. A Qt front-end can therefore reuse the
entire engine unchanged — which is what makes the rewrite realistic rather than a
multi-year rewrite of everything.

This is a fork. Upstream stays on wxWidgets, so GUI fixes there have to be carried
over by hand. Fixes in `core`/`wow` still merge normally, because those libraries are
untouched.

## Building

Requirements:

- Visual Studio 2022 Build Tools (MSVC 14.4x, x64)
- Qt 5.13.2, `msvc2017_64`
- vcpkg with the `x64-windows-static-md` triplet (provides GLEW)
- Autodesk FBX SDK 2020.3.9 — only needed to package `libfbxsdk.dll`

The upstream tree is a submodule, and its build products (`core.lib`, `wow.lib`,
`core.dll`, `wow.dll`, the four plugin DLLs) are what this project links against:

```
git clone --recurse-submodules https://github.com/LouisBunt/wowmodelviewer-qt.git
cd wowmodelviewer-qt

# Build the upstream tree first -- it produces build-x64\ with the import libraries.
cd upstream && Configure.bat && Build.bat && cd ..

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

If you keep the upstream tree elsewhere, override the paths:

```
cmake -S . -B build -DWMV_SRC=<tree> -DWMV_BUILD=<tree>/build-x64
```

Configure fails with a named cause if the source tree or either import library is
missing, rather than surfacing it as compile errors later.

## Running from the build tree

The executable resolves `database.xml`, the listfile and `.\plugins` **relative to the
working directory**, not to itself. Running it straight out of `build\Release` finds
none of them. Either stage a package (below) or run it from a directory that already
has that data. Both positional arguments are optional -- the install folder is
remembered in `userSettings\qt-frontend.ini` and asked for once if it cannot be found:

```
build\Release\WoWModelViewer-Qt.exe "C:\Program Files (x86)\World of Warcraft" 917116
```

Useful for checking things without touching the mouse:

| Flag | Effect |
|---|---|
| `--view <yaw>,<pitch>` | set the camera angle |
| `--equip <id>[,<id>…]` | equip items |
| `--category <0..3>` | browser category: all / characters / creatures / items |
| `--export <format>,<path>` | export without the dialog |
| `--shot <file.png>` | save the GL image after ~1.5 s and exit |

`--shot` exits with code 0 on success, which makes it usable as a smoke test.

## Packaging

`stage.ps1` assembles the payload, copying every file from whatever produces it —
build output, the Qt installation, the VC redist folder, the tracked `bin_support`
tree. Nothing is hand-copied, and the shipped DLL list is the dependency closure
reported by `dumpbin`, not inherited from the wx package.

```
powershell -ExecutionPolicy Bypass -File installer\stage.ps1
"%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" installer\BetterModelViewer.iss
```

The setup carries the same `AppId` as WoW Model Viewer Midnight, so it upgrades that
installation in place and removes its executable and wxWidgets DLLs.

## Licence

GPLv3, inherited from WoW Model Viewer. See [LICENSE](LICENSE).
