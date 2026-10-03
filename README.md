# Tom Clancy's Splinter Cell: Double Agent - XBE Recomp

A static recompilation of the original Xbox version of Splinter Cell: Double Agent into a native 64 bit Windows executable. The game's x86 code is translated to C++ ahead of time, the Xbox kernel, Direct3D and DirectSound calls are implemented on Windows, and everything is drawn with Direct3D 12 and played through XAudio2. Made with the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool, created for this game originally.

## Game Setup

- Put the game files in the `assets` folder.
- Run `SCDAOffline.exe`.
- You can adjust the recomp settings inside the in-game options menu whilst the game is running, or alternatively via the F10 quick menu.
<img width="1920" height="1080" alt="{3C71BA44-CD79-411A-90CB-7F88F80572B9}" src="https://github.com/user-attachments/assets/baec07ee-feb3-4748-a121-d1a5bf44e8d3" /> 


> [!NOTE]
> No game data, XBE bytes or generated code are in this repository. You must own the game.

## Features

### Display
- **Native Renderer** - The game renders through a native Direct3D 12 renderer, no emulation.
- **Display Mode** - Allows you to change between Fullscreen, Borderless and Windowed modes.
- **Resolution** - Lists every mode your monitor reports and defaults to the desktop mode, replacing the 640x480 lock.
- **Render Resolution** - Renders the game at 20% to 200% of the output resolution, for supersampling or downscaling, independent of the window size.
- **Max Frame Rate** - Caps the frame rate at your monitor's refresh rate, at a value you pick, or removes the cap, replacing the console's 30 FPS limiter.
- **V-Sync** - Enables V-Sync, reducing tearing.
- **FPS Counter** - Shows the frame rate on screen.

### Graphics
- **Async Shaders** - New effects are compiled on background threads instead of pausing the game, so one can appear a frame or two late the first time it is seen. Compiled shaders and pipelines are kept in the `shadercache` folder beside the exe and load at once from then on.
- **Anisotropic Filtering** - Forces the selected anisotropic filtering level on all textures.
- **Shadow Resolution** - Allows you to change the shadow resolution

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

- **HDR Rendering** - Enables shader model 3 HDR rendering.
- **Tone Mapping** - Enables shader model 3 tone mapping.
- **HQ Soft Shadows** - Enables shader model 3 soft shadows

### Mouse and Keyboard
- **Keyboard Support** - Fully supports keyboard and mouse input.
- **Raw Mouse** - Native raw mouse input driven straight into the camera.
- **Mouse Look Sensitivity** - Adjusts mouse look sensitivity.
- **Invert Mouse Look** - Inverts the vertical look axis.
- **Rebinding** - Every action can be rebound from the Mouse and Keyboard page.
- **Custom Menu Cursor** - Put a `cursor.png` (or `cursor.dds`) beside the exe to replace the Windows arrow in the game's menus.

### General
- **Skip Intro** - Skips the intro logo's.
- **Boot Splash** - Put a `SplinterCell4Logo.png` (or `.bmp`) beside the exe for a splash screen at launch.
- **COOP Support** - Allows for online COOP, with a custom join IP.
- **Versus Support** - The versus mode runs natively: profiles, tutorials and System Link over the LAN.

### Build

- Install Visual Studio 2022 with **Desktop development with C++**, **C++ Clang tools for Windows** and **C++ CMake tools for Windows** (build.bat offers to add the last two), plus Git.
- Clone this repository.
- Put the game files in the `assets` folder.
- Run `build.bat`. The first build recompiles the game and takes a while.
- Run `out\build\release\SCDAOffline.exe`.


