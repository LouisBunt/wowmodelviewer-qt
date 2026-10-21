# MViewer

**Model-Viewer für World of Warcraft** — a model viewer for World of Warcraft, on Qt. Up to
1.1 it was called *ModelViewer: Midnight*. It began as a new interface for
[WoW Model Viewer](https://github.com/wowmodelviewer/wowmodelviewer): the model, render and
data layer — `core.dll`, `wow.dll` and the plugins — comes from there and lives on in
[`engine/`](engine/README.md); the interface on top is new.

Released publicly at **1.0.0** and now at **1.5.0**, which added the online mode. The
early-access builds numbered 1.5–1.8 came before 1.0.0 and are a separate line. The
features below are tested and do what this says, but several areas of the old interface
are not ported yet. See
[LIESMICH.txt](installer/LIESMICH.txt) for the user-facing feature list and known gaps,
and [MIGRATION.md](MIGRATION.md) for why the port is shaped the way it is.

## What it does

- **Import a character from a Wowhead dressing-room link**, with equipment,
  customizations and the correct colour variants (those live in the trailing bonus ids).
  Dressing-room links are decoded locally — nothing is fetched for them. Saved outfits
  and transmog set pages are fetched. Verified against a 56-case oracle corpus
  (`tests/corpus`, run by `tests/whtest`).
- **Item view**: show one worn piece on its own, turn it around, export just that piece.
- **Export to Blender** as FBX with a `.wmvmat.json` sidecar, plus a bundled Blender
  add-on that rebuilds the material node graphs from it — alpha modes, glow and
  UV-scrolling effect planes included. The add-on installs itself from inside the app.
- **Export for the 3D printer** as a binary STL in millimetres: the figure as posed on
  screen, only the visible parts, scaled to a chosen height (a lone weapon or shield: its
  longest side), standing on the plate, glow and particle sheets left out. The file is read back and its measured size, footprint and
  the parts without thickness are reported (`--export STL,<path>`, `--print-height <mm>`).
  Thickening and welding stay in Blender, where the add-on's print pipeline does them.
- Item and transmog browser with categories, sorting and search, and filters that take
  several ticks at once (slot, expansion, armour class, quality); item sets can be
  applied on top of what is already worn.
- **One design system, not seven.** Colour, measure, type and motion live in `Theme.h`,
  the application has a single stylesheet, and `tools/qss-lint.ps1` fails the build if a
  raw hex value, a per-widget stylesheet or a point-size font creeps back in. Fonts (Inter,
  IBM Plex Mono, Cinzel) and icons (Lucide) ship with the application, so the layout is
  measured against the faces the user actually sees.
- **No WoW installation needed**: the online mode streams the game data from Blizzard's
  public CDN into a local cache on this machine, like wow.export's CDN mode. It is an explicit
  choice — in the first-start dialog, under Datei › Spieldaten-Quelle, or per run with
  `--online [region]` — never a silent download. The first start fetches about 400 MB (archive
  indexes and file tables, over parallel HTTPS, with a progress bar and a cancel button),
  every WoW patch about 280 MB more, and each model the first time it is shown; a start
  without network opens what is cached.
- Armory and NPC import, character save/load in the wx `.chr` format, light control,
  animation timeline, headless flags for scripted checks.

### The engine

`engine/` holds the model, render and data layer: `core.dll`, `wow.dll` and the five plugins.
It is WoW Model Viewer's, brought into this repository and developed here since. Its own
[README](engine/README.md) says from which state, what was left out and what has changed —
the online mode, the item view, the STL export and the FBX sidecar among it.

### The interface

`Theme.h/.cpp` is the design system: colour tokens by role, a 4px measure scale, a pixel type
scale and the one application stylesheet. `MViewerStyle` (a `QProxyStyle` over Fusion) supplies
what a stylesheet cannot reach — tree chevrons, pixel metrics, and the focus ring, which is shown
only for a widget the keyboard reached. `UiKit.h/.cpp` holds the parts the panels are built from:
`ElidedLabel` (shortens instead of clipping, tooltip only when something is hidden),
`SegmentedBar` (drops labels for icons rather than truncating them), `SearchField`, `PropertyRow`
(a label column measured over the real German strings, not a hardcoded width).

Scaling is the application's own: `ui::scale()` multiplies every measure and pixel font size.
Qt 5.13's `AA_EnableHighDpiScaling` is deliberately not used — it rounds the device pixel ratio to
a whole number, and `GLHost` reads its back buffer with `glReadPixels` over `width()/height()`,
which at a ratio above 1 captures a quarter of the frame. `--ui-scale` pins it for screenshots.

## History

The measurement that started it: of the four modules in WoW Model Viewer, three (`core`,
`games/wow`, `plugins`) contain zero wxWidgets includes. All 19,700 lines of toolkit code sat
in the executable alone. A Qt interface could therefore keep the entire engine — which is
what made the rewrite realistic rather than a multi-year rewrite of everything.

Until 1.5.0 the engine was a fork of WoW Model Viewer, included as a submodule. Since then it
is part of this repository and developed with it. WoW Model Viewer is no longer tracked; a fix
from there is ported by hand when it is worth having.

## Building

Requirements:

- Visual Studio 2022 Build Tools (MSVC 14.4x, x64)
- Qt 5.13.2, `msvc2017_64`
- vcpkg with `glew:x64-windows-static-md`, `libjpeg-turbo:x64-windows` and
  `libpng:x64-windows`
- Autodesk FBX SDK 2020.3.9, copied to `engine/ThirdParty/fbx_x64` (`include/`, and `lib/`
  with `libfbxsdk.lib` and `libfbxsdk.dll`) — the FBX exporter links it

Everything else is in the repository. The engine is built first, on its own; its build
products (`core.lib`, `wow.lib`, `core.dll`, `wow.dll`, the five plugin DLLs) are what the
application links against and ships:

```
git clone https://github.com/LouisBunt/MViewer.git
cd MViewer

cmake -S engine -B engine/build-x64 -G "Visual Studio 17 2022" -A x64
cmake --build engine/build-x64 --config Release

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Both builds look for Qt at `C:/Qt/Qt5.13.2/5.13.2/msvc2017_64` and for vcpkg under the
`VCPKG_ROOT` environment variable, so setting that once is usually enough. Otherwise:

```
cmake -S engine -B engine/build-x64 -G "Visual Studio 17 2022" -A x64 ^
      -DQT_LOCATION=<qt>/5.13.2/msvc2017_64 -DWMV_VCPKG_ROOT=<vcpkg>
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
      -DQT_LOCATION=<qt>/5.13.2/msvc2017_64 ^
      -DVCPKG_STATIC=<vcpkg>/installed/x64-windows-static-md
```

The application's four paths are all checked at configure time, so a wrong one fails with a
named cause instead of a wall of compile errors later:

| Variable | Default | What it needs to point at |
|---|---|---|
| `WMV_SRC` | `./engine` | the engine source tree |
| `WMV_BUILD` | `$WMV_SRC/build-x64` | its build output, holding `core.lib` and `wow.lib` |
| `QT_LOCATION` | `C:/Qt/Qt5.13.2/5.13.2/msvc2017_64` | a Qt 5.13 installation for MSVC x64 |
| `VCPKG_STATIC` | `$VCPKG_ROOT/installed/x64-windows-static-md` | a vcpkg triplet with GLEW |

The package also needs `listfile.csv`, the community listfile (about 150 MB, not in the
repository): `installer/stage.ps1` takes it from `engine/bin_support/wow/listfile.csv`, or
from `-Listfile <path>`. The current one is
<https://github.com/wowdev/wow-listfile/releases/latest/download/community-listfile.csv>.

## Running from the build tree

`database.xml`, the listfile, `.\plugins`, `wowdb.sqlite` and `userSettings\` are all
resolved **next to the executable** — `main()` pins the working directory to
`applicationDirPath()` before anything opens a file. Running straight out of
`build\Release` therefore still finds nothing, because that directory holds no data:
stage a package (below) and run it from there, or copy the fresh binaries over a staged
one. The flip side is that a *relative* `--shot` / `--export` path now lands next to the
executable rather than in the caller's directory — pass absolute paths in scripts.

Both positional arguments are optional; the data source (install folder or online) is
remembered in `userSettings\qt-frontend.ini` and asked for once if there is none. A lone
numeric first argument is the model id, with the folder taken from the settings:

```
build\Release\MViewer.exe "C:\Program Files (x86)\World of Warcraft" 917116
```

Useful for checking things without touching the mouse:

| Flag | Effect |
|---|---|
| `--view <yaw>,<pitch>` | set the camera angle |
| `--equip <id>[,<id>…]` | equip items |
| `--category <0..3>` | browser category: all / characters / creatures / items |
| `--dressing-room <url>` | import a Wowhead dressing-room link |
| `--focus <slot>` | item view: show only that slot's piece (-1 = off) |
| `--unequip <slot>[,…]` | take pieces off |
| `--export <format>,<path>` | export without the dialog |
| `--online [region]` | this run streams from Blizzard's CDN (`eu`, `us`, `kr`, `tw`, `cn`); never saved |
| `--locale <xxYY>` / `--cache <dir>` | data language and cache folder for an online run |
| `--shot <file.png>` | save the GL image after ~1.5 s and exit |

`--shot` exits with code 0 on success, which makes it usable as a smoke test. `--shot` and
`--export` also put the process in *scripted* mode: startup failures then report to
`userSettings\qt-frontend-trace.txt` and exit 1 instead of raising a modal dialog that
would hang the run. That includes a scripted run with no data source at all (no folder, no
`--online`, nothing remembered), where an interactive start would ask.

## Packaging

`stage.ps1` assembles the payload, copying every file from whatever produces it —
build output, the Qt installation, the VC redist folder, the tracked
`engine/bin_support` tree. Nothing is hand-copied, and the shipped DLL list is the dependency closure
reported by `dumpbin`, not inherited from the wx package.

The visible installer is not the Inno wizard: `installer/setupui` is a small Qt
program in the application's own design (same `Theme.h`), which the setup extracts
and shows instead. It collects folder and options, re-runs the setup with
`/VERYSILENT` and tails Inno's log for its progress display; all install and
uninstall mechanics stay in the engine. One command builds all of it — payload,
front-end, package:

```
powershell -ExecutionPolicy Bypass -File installer\build-setup.ps1
```

(`stage.ps1` + `ISCC installer\MViewer.iss` still work on their own; the driver
script also measures the payload so the front-end's progress bar divides by the real
file count. `/VERYSILENT /DIR=... /MERGETASKS=...` bypasses the front-end entirely
and behaves like any Inno setup.)

GitHub Actions runs the same build on a Windows runner for every push to `master` and
every pull request (`.github/workflows/windows-build.yml`): it fetches Qt, the vcpkg
packages, the FBX SDK and the listfile, builds the engine, the application and the setup,
runs the tests, and attaches `MViewer-Setup-<version>.exe` to the run as an artifact.
A tag `v<version>` publishes that setup as the GitHub release, with
`installer/RELEASE-<version>.md` as its notes and the file's size and SHA-256 underneath.
The tag can be pushed, or made on github.com by publishing a new release with it (target
`master`); the workflow then fills in that release.

The setup has its own `AppId`, so it upgrades its own earlier versions in place and leaves
any WoW Model Viewer installation alone. The files of one in the target folder are removed
only when the user ticks the option for it. An upgrade from *ModelViewer: Midnight* (1.0,
1.1) keeps that installation's folder, and with it the settings, looks and caches; the old
`WoWModelViewer-Qt.exe` and its shortcuts are removed in `[InstallDelete]`.

The MV mark and the splash in `resources/` are stand-ins drawn by `tools/make-logo.py`.
[resources/LOGO.md](resources/LOGO.md) says which files make up the logo and how to swap
them for a designed one.

## Reporting problems

<https://github.com/LouisBunt/MViewer/issues> — please attach
`userSettings\log.txt` (engine, archive and exporter messages) and
`userSettings\qt-frontend-trace.txt` (startup and imports, step by step). If the
application does not start at all, `userSettings\qt-frontend-error.txt` holds the reason.

## Licence

GPLv3, inherited from WoW Model Viewer. See [LICENSE](LICENSE).

The distributed package bundles third-party binaries and data — Autodesk's FBX SDK, Qt,
OpenSSL, the community listfile and the TACT keys. Their origins and terms are in
[THIRD-PARTY-NOTICES.txt](installer/THIRD-PARTY-NOTICES.txt), which ships inside the
package. Note that `fbxexporter.dll` is GPLv3 code linked against the proprietary FBX
SDK; see that file's FBX section before redistributing the binaries.

World of Warcraft is the property of Blizzard Entertainment. This program contains no
game data. It reads the installation on your own machine — or, only in the online mode,
which has to be chosen explicitly, it downloads the files it needs on demand from
Blizzard's public CDN into a local cache on your machine. Those files remain Blizzard's
property and are never redistributed, neither by the program nor by this project. Not
affiliated with Blizzard.

---
<sub>Teilweise mit KI-Unterstützung (Claude) entwickelt.</sub>
