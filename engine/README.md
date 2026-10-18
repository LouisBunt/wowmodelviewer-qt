# Engine

The model, render and data layer under the application: `core.dll`, `wow.dll` and the five
plugins (FBX, OBJ and STL export, Armory and Wowhead import), together with the libraries they
are built from, the data definitions they read and the Blender add-on that goes with the FBX
export.

## Where it comes from

This is code from [WoW Model Viewer](https://github.com/wowmodelviewer/wowmodelviewer),
© WoW Model Viewer Community, under the GPLv3. It was brought into this repository on
2026-10-18 from the fork it had been developed in until then, at commit `62f94f3`
(2026-09-26): WoW Model Viewer's `develop` branch at `adb2cb1` (2026-07-23), plus the 19
commits listed below. Since then it is developed here, as part of this repository, and is no
longer kept in step with WoW Model Viewer; a fix from there is ported by hand when it is worth
having.

The licence stays the GPLv3, WoW Model Viewer's and this repository's ([LICENSE](../LICENSE)).
The libraries under `ThirdParty/` keep their own licences, which travel with them.

## What was left out

Only what the application builds and ships came across:

- `Source/wowmodelviewer`, the wxWidgets program. Its one toolkit-free class, `OrbitCamera`,
  now lives with the application (`../OrbitCamera.cpp`).
- With it wxWidgets, the 32-bit configuration and its prebuilt libraries (`ThirdParty/include`,
  `lib`, `libs`), the NSIS installer, `ListFileGenerator`, the AppVeyor and batch build scripts,
  the Linux and macOS CMake files and the documentation folders.
- From `bin_support`: the wx program's splash screens, icons and translations, and the 32-bit
  Visual C++ runtime.
- CascLib's bundled `listfile` (90 MB). The build fetches the current community listfile
  instead.
- WoW Model Viewer's `CHANGELOG.md`, which describes the wx program.

## What changed compared to WoW Model Viewer

The 19 commits between `adb2cb1` and `62f94f3`, by area:

| Where | Change |
|---|---|
| `CASCFolder::initOnline` / `setConfig` / `errorText`, `WoWFolder::setOnline` | online mode: `versions` and `cdns` come from Blizzard's version service over HTTPS and stay in the cache, so a start without network opens the cached build; the region's CDN hosts get four Blizzard fallbacks; the storage opens from the cache with `CascOpenStorageEx` and never touches port 1119 |
| `onlineRefreshed` / `cancelOnlineOpen` / progress callback in `CASCFolder`, `WoWFolder` | the application can tell an offline start from a fresh one, show the whole online open as one progress bar, and cancel a first download |
| `CASCFolder::addExtraEncryptionKeys` | online, wowdev's TACT key list is refreshed over HTTPS (a conditional request, cached for offline starts) and added to the shipped `extraEncryptionKeys.csv` |
| `CDNPrefetch` (new) | before the open, the build and CDN configs, the archive indexes and the encoding table are fetched over parallel HTTPS, verified and written into CascLib's cache layout, instead of one after another over a single connection |
| `ThirdParty/casclib` | one patch set on the vendored CascLib: an archived file is fetched with an HTTP range request instead of its whole archive, every download is verified before it enters the cache and written under a temporary name, sockets time out, online opens skip the download manifest — file by file in `ThirdParty/casclib/LOCAL-CHANGES.md` |
| `GameDatabase::initFromXML` | the database cache is keyed by build **and** data language (`build\|locale\|schemaN`), so a change of the online language rebuilds it |
| `WoWModel::initAnimated` | a variant skeleton's own animations were discarded in favour of the base skeleton's, so the upright orc behaved exactly like the model it varies |
| `wdc3file` | sparse DB2 fields are read |
| `WoWModel::setItemFocus` / `applyItemFocus` / `visibleBounds` / `mergedGeosetRanges`, `WoWItem::mergedModel()` | the item view: hiding everything but one worn piece, and reporting what is left for the camera |
| `RaceInfos`, `CharDetails` | the Armory import finds races that share their M2 with another race |
| `TextureGroup.h`, `SceneLighting.h` (new) | lifted out of the wx program's headers, so the FBX exporter and the application need none of them |
| `FBXHeaders::createMesh`, `FBXExporter` | vertex remapping, so an export writes only the vertices a visible pass uses; texture paths fixed; hidden items and a hidden character stay out of the file |
| `FBXExporter` sidecar | bone names, attachments, animation metadata, UV-scroll tracks |
| `plugins/exporters/stl` (new) | STL export for the 3D printer: the figure as posed, in millimetres, standing on the plate |
| `blender_addon` | one-click import, material node graphs rebuilt from the sidecar, effect planes, UV scrolling, the print pipeline and its tests |
| `bin_support/wow/12.0/database.xml`, `bin_support/dbd/ItemBonus.dbd` | the sparse fields, and `ItemBonus`, so a dressing-room import can resolve its colour variants |
| `bin_support/openssl` (new) | OpenSSL 1.1 for Qt's HTTPS, shipped with the package |
| `Source/games/wow/CMakeLists.txt` | `wow.dll` builds `CDNPrefetch.cpp` and links `Qt5::Network` |

And with the move into this repository:

| Where | Change |
|---|---|
| `CMakeLists.txt`, `cmake/common.cmake`, `ThirdParty/CxImage`, `plugins/exporters/fbx` | x64 only; no wxWidgets, no 32-bit branches, no NSIS target; vcpkg is found from `VCPKG_ROOT` |
| `Source/CMakeLists.txt` | `Source/wowmodelviewer` is no longer built |

## Building

On its own, before the application, which links its import libraries:

```
cmake -S engine -B engine/build-x64 -G "Visual Studio 17 2022" -A x64 ^
      -DQT_LOCATION=<qt>/5.13.2/msvc2017_64 -DWMV_VCPKG_ROOT=<vcpkg>
cmake --build engine/build-x64 --config Release
```

The FBX exporter needs the FBX SDK 2020.3 under `ThirdParty/fbx_x64` (`include/`, and
`lib/libfbxsdk.lib` with `libfbxsdk.dll`); it is not in the repository. The top-level README
lists everything else the build needs.
