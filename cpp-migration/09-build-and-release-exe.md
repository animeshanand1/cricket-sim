# 09: Build and release the .exe

Goal: one file, `T20Coach.exe`, that anyone can download and double-click on Windows 10/11 x64, with no installer required, no DLLs, and no runtime to install.

---

## 1. One-time setup (developer PC)

1. **Visual Studio 2022** (Community is free) with the workload **Desktop development with C++** (gives MSVC, the Windows SDK, CMake and Ninja).
2. **Git**.
3. **vcpkg**:
   ```powershell
   git clone https://github.com/microsoft/vcpkg C:\dev\vcpkg
   C:\dev\vcpkg\bootstrap-vcpkg.bat
   [Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\dev\vcpkg", "User")
   ```
4. Open a new terminal so `VCPKG_ROOT` is set.

## 2. Project files

### `vcpkg.json`

```json
{
  "name": "t20coach",
  "version": "3.0.0",
  "dependencies": [
    "sdl3",
    { "name": "imgui", "features": ["sdl3-binding", "opengl3-binding", "freetype"] },
    "implot",
    "nanosvg",
    "nlohmann-json",
    "tsl-ordered-map",
    "catch2"
  ],
  "builtin-baseline": "<commit id: run `vcpkg x-update-baseline --add-initial-baseline` once>"
}
```

### `CMakePresets.json`

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "win-release",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/release",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "VCPKG_TARGET_TRIPLET": "x64-windows-static",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded"
      }
    },
    {
      "name": "win-debug",
      "inherits": "win-release",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreadedDebug"
      }
    }
  ],
  "buildPresets": [
    { "name": "win-release", "configurePreset": "win-release" },
    { "name": "win-debug", "configurePreset": "win-debug" }
  ]
}
```

`x64-windows-static` + `MultiThreaded` (`/MT`) = every library and the C runtime are linked into the exe. Nothing to install on the player's PC (OpenGL comes with Windows).

### `CMakeLists.txt` (skeleton)

```cmake
cmake_minimum_required(VERSION 3.25)
project(T20Coach VERSION 3.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /fp:precise)   # /fp:precise: keep JS-like floating point
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(tsl-ordered-map CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)
find_path(NANOSVG_INCLUDE_DIRS "nanosvg.h")
find_package(OpenGL REQUIRED)

file(GLOB_RECURSE CORE_SRC   src/core/*.cpp)
file(GLOB_RECURSE ENGINE_SRC src/engine/*.cpp)
file(GLOB_RECURSE STATE_SRC  src/state/*.cpp)
file(GLOB_RECURSE UI_SRC     src/ui/*.cpp)

add_library(t20_core STATIC ${CORE_SRC})
target_include_directories(t20_core PUBLIC src)
target_link_libraries(t20_core PUBLIC tsl::ordered_map)

add_library(t20_engine STATIC ${ENGINE_SRC})
target_link_libraries(t20_engine PUBLIC t20_core)

add_library(t20_state STATIC ${STATE_SRC})
target_link_libraries(t20_state PUBLIC t20_engine nlohmann_json::nlohmann_json SDL3::SDL3)

include(cmake/embed_assets.cmake)          # generates ${CMAKE_BINARY_DIR}/assets_embedded.cpp
embed_assets(EMBEDDED_SRC
  assets/fonts/Barlow-Regular.ttf assets/fonts/Barlow-Medium.ttf assets/fonts/Barlow-SemiBold.ttf
  assets/fonts/Barlow-Bold.ttf assets/fonts/BarlowCondensed-Bold.ttf
  assets/fonts/BarlowCondensed-ExtraBold.ttf assets/fonts/BarlowCondensed-ExtraBoldItalic.ttf
  assets/svg/hero.svg)

add_executable(T20Coach WIN32 src/main.cpp ${UI_SRC} ${EMBEDDED_SRC} assets/T20Coach.rc)
target_include_directories(T20Coach PRIVATE ${NANOSVG_INCLUDE_DIRS})
target_link_libraries(T20Coach PRIVATE t20_state imgui::imgui implot::implot OpenGL::GL)

add_executable(t20coach_balance tools/balance.cpp)
target_link_libraries(t20coach_balance PRIVATE t20_engine)

file(GLOB_RECURSE TEST_SRC tests/*.cpp)
add_executable(t20_tests ${TEST_SRC})
target_link_libraries(t20_tests PRIVATE t20_state Catch2::Catch2WithMain)
include(CTest)
include(Catch)
catch_discover_tests(t20_tests)
```

`WIN32` makes it a windowed app (no console). SDL3's `SDL_main.h` provides the Windows entry point; keep `int main(int, char**)`.

### Assets

`cmake/embed_assets.cmake` turns each file into a `const unsigned char[]` plus size, so the exe carries the fonts and the stadium art. (A 10-line CMake function using `file(READ … HEX)` and `string(REGEX REPLACE …)`; or use Windows `RCDATA` resources if you only ever target Windows.) Icons and team badges are generated from SVG strings in code, so they need no files.

### `assets/T20Coach.rc` (icon and version info)

```rc
IDI_APP ICON "icons/app.ico"
1 VERSIONINFO
 FILEVERSION 3,0,0,0
 PRODUCTVERSION 3,0,0,0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName", "T20 Coach"
      VALUE "FileDescription", "T20 Coach - cricket management"
      VALUE "FileVersion", "3.0.0"
      VALUE "ProductName", "T20 Coach"
      VALUE "ProductVersion", "3.0.0"
      VALUE "OriginalFilename", "T20Coach.exe"
    END
  END
  BLOCK "VarFileInfo" BEGIN VALUE "Translation", 0x409, 1200 END
END
```

Make `app.ico` from `t20-coach/public/icons/icon.svg` with sizes 16, 24, 32, 48, 64, 128, 256 (any icon tool, or ImageMagick: `magick icon.svg -define icon:auto-resize=256,128,64,48,32,24,16 app.ico`).

## 3. Build commands

```powershell
cd t20-coach-v1.8\t20-coach-cpp
cmake --preset win-release          # first run downloads and builds the libraries (10-20 min)
cmake --build --preset win-release
ctest --test-dir build\release      # engine tests
build\release\t20coach_balance.exe 3000
build\release\T20Coach.exe
```

Debug build: `--preset win-debug`. In Visual Studio: **File → Open → Folder**, pick `t20-coach-cpp`; it reads the presets.

## 4. Release checklist

| Step | Check |
|------|-------|
| 1 | Version bumped in `vcpkg.json`, `project(... VERSION)`, `T20Coach.rc`, the About box |
| 2 | `ctest` green; balance tool within tolerance (see [10](10-testing-and-parity.md)) |
| 3 | Release build, then run `dumpbin /dependents build\release\T20Coach.exe`: only Windows system DLLs (`KERNEL32`, `USER32`, `GDI32`, `OPENGL32`, `SHELL32`, `ADVAPI32`, `IMM32`, `WINMM`, `VERSION`, `SETUPAPI`, `OLE32`, `OLEAUT32`…), **no** `MSVCP140`, `VCRUNTIME140`, `SDL3.dll` |
| 4 | Test on a clean Windows VM (Windows Sandbox works): double-click, play a friendly, quit, relaunch, career still there |
| 5 | Test with a high-DPI display at 150% and 200% |
| 6 | Package (below) |

## 5. Packaging options

### A. Plain zip (simplest)

```
T20Coach-3.0.0-win64.zip
├─ T20Coach.exe
├─ README.txt             how to play, where saves live, how to uninstall (delete the folder + %APPDATA%\T20Coach)
└─ licenses/
   ├─ OFL-Barlow.txt       required by the SIL Open Font License when you redistribute the fonts
   └─ THIRD-PARTY.txt      SDL (zlib), Dear ImGui (MIT), ImPlot (MIT), nanosvg (zlib), nlohmann/json (MIT), tsl ordered-map (MIT), Catch2 (BSL, tests only)
```

### B. Installer (Start menu shortcut, uninstaller)

Inno Setup (free). `installer/T20Coach.iss`:

```ini
[Setup]
AppName=T20 Coach
AppVersion=3.0.0
DefaultDirName={autopf}\T20 Coach
DefaultGroupName=T20 Coach
OutputBaseFilename=T20Coach-3.0.0-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\t20-coach-cpp\assets\icons\app.ico
UninstallDisplayIcon={app}\T20Coach.exe

[Files]
Source: "..\t20-coach-cpp\build\release\T20Coach.exe"; DestDir: "{app}"
Source: "licenses\*"; DestDir: "{app}\licenses"

[Icons]
Name: "{group}\T20 Coach"; Filename: "{app}\T20Coach.exe"
Name: "{autodesktop}\T20 Coach"; Filename: "{app}\T20Coach.exe"; Tasks: desktopicon

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"
```

Saves stay in `%APPDATA%`, so uninstalling keeps the career (tell players this).

## 6. Code signing and SmartScreen

An unsigned exe downloaded from the internet shows **"Windows protected your PC"** until it builds reputation. Options:

| Option | Cost | Effect |
|--------|------|--------|
| Ship unsigned | Free | Players click **More info → Run anyway**. Say so on the download page |
| Azure Trusted Signing | Low monthly fee | Signs with a Microsoft-issued certificate; reputation builds quickly |
| OV / EV code-signing certificate | Yearly fee | Standard route; EV gets instant SmartScreen trust |

Sign with `signtool sign /fd SHA256 /tr <timestamp-url> /td SHA256 ...` after building, before zipping or building the installer. Sign the installer too.

Antivirus false positives: static, unsigned, newly built exes sometimes trip heuristics. Signing helps most; you can also submit the file to Microsoft (Defender) for analysis.

## 7. Where to publish

GitHub Releases (attach the zip or installer), itch.io (made for games; has a desktop app that handles updates), or your own site. Publish a SHA-256 of each file.

## 8. Before you publish: names and likenesses

The roster uses **altered** names of real franchises and players (see `t20-coach/README.md`, "The league and its names"). The original author's own note: *altered names are not a substitute for a licence; get legal advice on player and team likenesses before publishing, or move to clearly different names and logos.* All names live in one file (`roster.js` → `roster.cpp`), so a fully fictional league is a one-file change.

## 9. Optional: CI build

A GitHub Actions workflow on `windows-latest` can run `cmake --preset win-release`, build, run `ctest`, and upload `T20Coach.exe` as an artifact on every push. Cache vcpkg binaries (`VCPKG_BINARY_SOURCES` with the GitHub Actions cache) to keep builds under 10 minutes.

## 10. Other platforms (later)

The stack (SDL3, ImGui, OpenGL, nlohmann/json) is cross-platform. macOS (`.app`, needs signing and notarisation) and Linux (AppImage) builds need only new presets and packaging; no code changes beyond the save path, which SDL already handles.
