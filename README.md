# ModelViewer: Midnight

A Qt front-end for [WoW Model Viewer](https://github.com/wowmodelviewer/wowmodelviewer),
replacing the wxWidgets one. The model, render and data layers are reused rather than
rewritten: `core.dll` and `wow.dll` are the upstream libraries, and the GUI on top is new.

Released as **1.7.0** (early access) — the features below are tested and do what
this says, but several areas of the old interface are not ported yet. See
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
- Item and transmog browser with categories, sorting and search; item sets can be
  applied on top of what is already worn.
- Armory and NPC import, character save/load in the wx `.chr` format, light control,
  animation timeline, headless flags for scripted checks.

### Engine changes carried in the submodule

"Reuse the engine unchanged" was true at 1.5.0 and is no longer. These live in
`upstream/` and have to be carried across upstream merges:

| Where | Change |
|---|---|
| `WoWModel::initAnimated` | a variant skeleton's own animations were discarded in favour of the base skeleton's, so the upright orc behaved exactly like the model it varies |
| `WoWModel::setItemFocus` / `applyItemFocus` / `visibleBounds` / `mergedGeosetRanges` | the item view — hiding everything but one worn piece, and reporting what is left for the camera |
| `WoWItem::mergedModel()` | accessor the item view needs to tell merged geometry from attached models |
| `FBXHeaders::createMesh`, `FBXExporter` | vertex remapping, so an export writes only the vertices a visible pass uses instead of every vertex in the model |
| `FBXExporter` sidecar | bone names, attachments, animation metadata, UV-scroll tracks |

## Why this exists

The measurement that started it: of the four modules in the upstream tree, three
(`core`, `games/wow`, `plugins`) contain zero wxWidgets includes. All 19,700 lines of
toolkit code sit in the executable alone. A Qt front-end can therefore reuse the
entire engine unchanged — which is what makes the rewrite realistic rather than a
multi-year rewrite of everything.

This is a fork. Upstream stays on wxWidgets, so GUI fixes there have to be carried over
by hand. Fixes in `core`/`wow` mostly still merge normally — the exceptions are the
engine changes listed above, which touch `games/wow` and the FBX plugin and have to be
re-applied if upstream ever rewrites those files.

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

Four paths are configurable, and all four are checked at configure time so a wrong one
fails with a named cause instead of a wall of compile errors later:

| Variable | Default | What it needs to point at |
|---|---|---|
| `WMV_SRC` | `./upstream` | the WMV source tree (the submodule) |
| `WMV_BUILD` | `$WMV_SRC/build-x64` | its build output, holding `core.lib` and `wow.lib` |
| `QT_LOCATION` | `C:/Qt/Qt5.13.2/5.13.2/msvc2017_64` | a Qt 5.13 installation for MSVC x64 |
| `VCPKG_STATIC` | `$VCPKG_ROOT/installed/x64-windows-static-md` | a vcpkg triplet with GLEW |

```
cmake -S . -B build -DWMV_SRC=<tree> -DWMV_BUILD=<tree>/build-x64 ^
      -DQT_LOCATION=<qt>/5.13.2/msvc2017_64 ^
      -DVCPKG_STATIC=<vcpkg>/installed/x64-windows-static-md
```

`VCPKG_STATIC` defaults from the `VCPKG_ROOT` environment variable, so setting that once
is usually enough:

```
vcpkg install glew:x64-windows-static-md
```

## Running from the build tree

`database.xml`, the listfile, `.\plugins`, `wowdb.sqlite` and `userSettings\` are all
resolved **next to the executable** — `main()` pins the working directory to
`applicationDirPath()` before anything opens a file. Running straight out of
`build\Release` therefore still finds nothing, because that directory holds no data:
stage a package (below) and run it from there, or copy the fresh binaries over a staged
one. The flip side is that a *relative* `--shot` / `--export` path now lands next to the
executable rather than in the caller's directory — pass absolute paths in scripts.

Both positional arguments are optional; the install folder is remembered in
`userSettings\qt-frontend.ini` and asked for once if it cannot be found:

```
build\Release\WoWModelViewer-Qt.exe "C:\Program Files (x86)\World of Warcraft" 917116
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
| `--shot <file.png>` | save the GL image after ~1.5 s and exit |

`--shot` exits with code 0 on success, which makes it usable as a smoke test. Any of
these flags also puts the process in *scripted* mode: startup failures then report to
`userSettings\qt-frontend-trace.txt` and exit 1 instead of raising a modal dialog that
would hang the run.

## Packaging

`stage.ps1` assembles the payload, copying every file from whatever produces it —
build output, the Qt installation, the VC redist folder, the tracked `bin_support`
tree. Nothing is hand-copied, and the shipped DLL list is the dependency closure
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

(`stage.ps1` + `ISCC installer\MVMidnight.iss` still work on their own; the driver
script also measures the payload so the front-end's progress bar divides by the real
file count. `/VERYSILENT /DIR=... /MERGETASKS=...` bypasses the front-end entirely
and behaves like any Inno setup.)

The setup carries the same `AppId` as WoW Model Viewer Midnight, so it upgrades that
installation in place and removes its executable and wxWidgets DLLs.

## Reporting problems

<https://github.com/LouisBunt/wowmodelviewer-qt/issues> — please attach
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
game data — it reads the installation on your own machine. Not affiliated with Blizzard.
