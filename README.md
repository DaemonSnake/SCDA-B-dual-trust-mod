# Tom Clancy's Splinter Cell: Double Agent - V2 Recomp

A static recompilation of the original Xbox version of Splinter Cell: Double Agent into a native 64 bit Windows executable. The game's x86 code is translated to C++ ahead of time, the Xbox kernel, Direct3D and DirectSound calls are implemented on Windows, and everything is drawn with Direct3D 12 and played through XAudio2.

## Installation

1. Install Visual Studio 2022 with **Desktop development with C++**, **C++ Clang tools for Windows** and **C++ CMake tools for Windows** (build.bat offers to add the last two), plus Git.
2. Clone this repository.
3. Put the game files in the `assets` folder (see Game Setup).
4. Run `build.bat`. The first build recompiles the game and takes a while.
5. Run `out\build\win-amd64-release\SCDA.exe`.

> [!NOTE]
> No game data, XEX bytes or generated code are in this repository. You must own the game.

## Features

### Display
- **Native Renderer** - The game renders through a native Direct3D 12 renderer, no emulation.
- **Display Mode** - Allows you to change between Fullscreen, Borderless and Windowed modes.
- **Resolution** - Lists every mode your monitor reports and defaults to the desktop mode, replacing the 1280x720 lock.
- **Render Resolution** - Renders the game at 20% to 200% of the output resolution, for supersampling or downscaling, independent of the window size.
- **Max Frame Rate** - Caps the frame rate at your monitor's refresh rate, at a value you pick, or removes the cap, replacing the console's 30 FPS limiter.
- **FPS Counter** - Shows the frame rate on screen.
- 
### Graphics
- **Shader Model** - Chooses between Shader Model 1.1, the console's look, and Shader Model 3.0, which turns on the Chaos Theory PC style features.
- **HDR Rendering** - 
- **Tone Mapping** - 
- **HQ Soft Shadows** - 

### Mouse and Keyboard
- **Keyboard Support** - Fully supports keyboard and mouse input.
- **Raw Mouse** - Native raw mouse input driven straight into the camera.
- **Mouse Look Sensitivity** - Adjusts mouse look sensitivity.
- **Invert Mouse Look** - Inverts the vertical look axis.
- **Rebinding** - Every action can be rebound from the Mouse and Keyboard page.

### General


