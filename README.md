# Tom Clancy's Splinter Cell: Double Agent - V2 Recomp

A static recompilation of the original Xbox version of Splinter Cell: Double Agent into a native 64 bit Windows executable. The game's x86 code is translated to C++ ahead of time, the Xbox kernel, Direct3D and DirectSound calls are implemented on Windows, and everything is drawn with Direct3D 12 and played through XAudio2.

## Installation

1. Install Visual Studio 2022 with **Desktop development with C++**, **C++ Clang tools for Windows** and **C++ CMake tools for Windows** (build.bat offers to add the last two), plus Git.
2. Clone this repository.
3. Put the game files in the `assets` folder (see Game Setup).
4. Run `build.bat`. The first build recompiles the game and takes a while.
5. Run `out\build\win-amd64-release\SCDAOffline.exe`.

> [!NOTE]
> No game data, XEX bytes or generated code are in this repository. You must own the game.

## Game Setup

Copy the game disc's files into `assets`: `default.xbe` and `sc4_adv.xbe` with the `Default`, `Media` and `adv` folders beside them. `default.xbe` becomes `SCDAOffline.exe` (story and co-op), `sc4_adv.xbe` becomes `SCDAOnline.exe` (versus). Picking Versus in the main menu switches to `SCDAOnline.exe` and leaving versus switches back.

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

### Shader Model 3
- **Shader Model** - Chooses between Shader Model 1.1, the console's look, and Shader Model 3.0, which turns on the Chaos Theory PC style features.
<div align="center">
  <table>
    <tr>
      <td width="50%"><img style="width:100%" src="https://github.com/user-attachments/assets/52b7ed68-cb87-466e-847b-8364589a6d8b"></td>
      <td width="50%"><img style="width:100%" src="https://github.com/user-attachments/assets/d82cc615-ea9a-4621-991c-663e7d3bd242"></td>
    </tr>
    <tr>
      <td align="center">Off</td>
      <td align="center">On</td>
    </tr>
  </table>
</div>

- **HDR Rendering** - 
- **Tone Mapping** - 
- **HQ Soft Shadows** - 

### Mouse and Keyboard
- **Keyboard Support** - Fully supports keyboard and mouse input.
- **Raw Mouse** - Native raw mouse input driven straight into the camera.
- **Mouse Look Sensitivity** - Adjusts mouse look sensitivity.
- **Invert Mouse Look** - Inverts the vertical look axis.
- **Rebinding** - Every action can be rebound from the Mouse and Keyboard page.
- **Custom Menu Cursor** - Put a `cursor.png` (or `cursor.dds`) beside the exe to replace the Windows arrow in the game's menus.

### General
- **Boot Splash** - Put a `SplinterCell4Logo.png` (or `.bmp`) beside the exe for a splash screen at launch.
- **Versus** - The versus mode runs natively: profiles, tutorials and System Link over the LAN.


