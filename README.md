# CubeDemoWork

## Engine version

`Cocos2d-x 3.17`

## Windows: prerequisites

- **Windows10** or later (tested workflow; other versions may work).
- **Visual Studio 2019** or newer with the **Desktop development with C++** workload.
- **Win32 (x86)** build: the supplied solution is configured for **Win32**, not x64.
- A clone of this repo that includes the **`cocos2d`** folder (engine sources next to `proj.win32`).

The project file selects the **v142** toolset on Visual Studio 2019 (`VisualStudioVersion` 16.0) and **v143** on Visual Studio 2022 (17.0), when those versions are installed.

## Windows: build and run

1. Open **`proj.win32/CubeDemoWork.sln`** in Visual Studio.
2. Set the active solution configuration to **Debug** or **Release** and the platform to **Win32**.
3. Set **CubeDemoWork** as the startup project (right‑click the project → *Set as Startup Project*).
4. Build the solution (**Build → Build Solution**). This also builds **libcocos2d** and other dependencies referenced by the solution.
5. Run with **Debug → Start Without Debugging** or **Start Debugging**.

The executable is written under the solution directory, for example:

- `proj.win32/Debug.win32/CubeDemoWork.exe`
- `proj.win32/Release.win32/CubeDemoWork.exe`

A **Custom Build Step** on the app project copies the **`Resources`** folder into that output directory so assets are next to the `.exe`. If resources are missing at runtime, copy `Resources` manually into the same folder as `CubeDemoWork.exe`.

## Track background

The scene loads **`Resources/track_background.png`** (squircle board with a yellow arrow lane). Replace that file with your own square artwork if you like. Path alignment is in `GameScene::setupBackgroundAndPath()`: **`kHalfNormOuter`** sets top/bottom half‑width, **`kHalfNormSide`** sets how far in from the sides the left/right straights sit (smaller = closer to the yellow center line on those edges). Adjust **`kCornerNorm`** for corner radius.
