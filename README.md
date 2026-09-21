# NVVR

NVVR is an early proof of concept for seated, head-tracked VR in Fallout: New Vegas. It ports selected ideas from OBVR 0.1.3 to the New Vegas engine and targets gamepad or keyboard-and-mouse play without hand tracking.

This is an early development build for engineering research only. It is not intended for public use, distribution, or unsupervised play. Expect missing features, instability, and incompatible game or runtime setups.

## Current scope

- Automatic seated VR startup with head aiming.
- Stereo world rendering and room-anchored 2D UI layers.
- Main menu, in-game menus, dialogue, HUD, and Pip-Boy routing work in progress.
- Keyboard, mouse, and gamepad input; no hand tracking.

The project is a PoC, not a finished mod and not a claim of feature parity with OBVR.

## Build

Requirements:

- Windows x86
- Visual Studio 2022 Build Tools with the C++ workload
- CMake 3.20 or newer

Run `build.ps1` from a Visual Studio developer environment. It configures, builds, and runs the test suite:

```powershell
./build.ps1
```

The resulting NVSE plugin is `build/Release/NVVR.dll`. Installation into a game directory is intentionally left to the developer.

## License

NVVR is released under the GNU General Public License v3.0. See [LICENSE](LICENSE). Third-party notices and source licenses remain with their respective files.
