# Splinter Cell: Double Agent - XBE Recomp
An unofficial native PC port of the original Xbox version of Splinter Cell: Double Agent, with extended graphics options, keyboard and mouse support, online co-op and Versus with voice chat, and other enhancements.

This port was created using the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build.

## Installation
- Put your game files in the `assets` folder. This can be an ISO, or the extracted data files.
- Run `SCDA.exe`.
- Recomp settings can be adjusted in the in-game options menu while the game is running, or through the Settings overlay, which is opened with F10 by default.

## Features
### Display
- **Display Mode** - Switch between Fullscreen, Borderless and Windowed.
- **Resolution** - Lists every mode your monitor reports and defaults to your desktop resolution, replacing the original 640x480 resolution.
- **Render Resolution** - Renders the game at 20% to 200% of the output resolution, allowing supersampling or downscaling independent of the window size.
- **Field of View** - Sets the horizontal field of view during gameplay from 45 to 140 degrees, with 75 being the game's default. Cutscenes, scopes, gadget views and lock picking keep the original.
- **Max Frame Rate** - Caps the frame rate to your monitor's refresh rate or a custom value, or removes the cap entirely, replacing the Xbox's 30 FPS limiter.
- **V-Sync** - Reduces screen tearing.
- **FPS Counter** - Displays the frame rate on screen.

### Graphics
- **Multisample Antialiasing (MSAA)** - Enables 2x, 4x or 8x MSAA to smooth jagged edges while preserving a sharp image. MSAA does not smooth alpha-tested edges such as fences and foliage.
- **Subpixel Morphological Antialiasing (SMAA)** - Enables SMAA to smooth jagged edges across the whole frame with a softer image. SMAA also smooths alpha-tested edges such as fences and foliage, and can be combined with MSAA.
- **Anisotropic Filtering** - Forces the selected anisotropic filtering level on all textures.
- **Shadow Resolution** - Raises the shadow resolution from the Xbox's 512x512 (default) up to 8192x8192.
- **Shadow Distance** - Increases the distance at which lights and their shadows turn off by up to 8x.

### Shader Model 3
- **Shader Model** - Choose between Shader Model 1.1, the Xbox look, and Shader Model 3.0, which enables the Chaos Theory PC style features.
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

- **HDR Rendering** - Enables Shader Model 3 HDR rendering.
- **Tone Mapping** - Enables Shader Model 3 tone mapping.
- **High Quality Soft Shadows** - Enables Shader Model 3 soft shadows.
- **Ambient Occlusion** - Adds screen space ambient occlusion, creating soft contact shadows where surfaces meet.

### Mouse and Keyboard
Full keyboard and mouse support with raw input, adjustable sensitivity, inverted look and rebindable controls.

<img width="1280" height="720" alt="Mouse and Keyboard" src="https://github.com/user-attachments/assets/baec07ee-feb3-4748-a121-d1a5bf44e8d3"/>

### Controller
Controller buttons can be rebound from the Controller page. Defaults match the Xbox layout.

<img width="1280" height="720" alt="Controller" src="https://github.com/user-attachments/assets/3b344418-9592-4e8d-b165-f64bdee8c311" />

### General
- **Skip Intro** - Skips the intro logos.
- **Co-op Support** - Co-op can be played online by directly connecting to a friend's IP address, over LAN, or in split-screen.
- **Co-op Tick Rate** - Raises the co-op update rate, which is capped at 22 Hz in the original game, up to 142 Hz. Both players must use the same value.
- **Co-op Min Input Delay** - Lowers the minimum input delay in co-op from the original 50 ms down to 0 ms. Both players must use the same value.

### Miscellaneous
- **Boot Splash** - Place a `SplinterCell4Logo.png` (or `.bmp`) beside the exe to display a splash screen at launch.
- **Custom Menu Cursor** - Place a `cursor.png` (or `cursor.dds`) beside the exe to replace the Windows arrow in the game's menus.

## System Requirements
### Minimum
- **OS**
  - Windows 10 64-bit
- **CPU** with 2 cores and support for SSE4.2 and POPCNT
  - Intel Nehalem (Intel Core 1st Generation)
  - AMD Bulldozer (AMD FX series)
- **GPU** with support for Direct3D 12
  - NVIDIA GeForce GT 630 (Kepler)
  - AMD Radeon HD 7750
  - Intel HD Graphics 510 (Skylake)
- **Memory**
  - 4 GB
- **Storage**
  - 5 GB for the game files, saves and shader cache

## Building
### 1. Clone the Repository
Clone this repository using [Git](https://git-scm.com/).
```
git clone https://github.com/TGP482/SCDoubleAgent-XBERecomp.git
```

### 2. Add the Required Game Files
Place your extracted game data files in the `assets` folder. `default.xbe` and `sc4_adv.xbe` must be there, with the `Default`, `Media` and `adv` folders beside them.

> [!NOTE]
> Building does not support the ISO format, so the files must be extracted. ISOs only work for playing.

### 3. Install Dependencies
Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) and select the following **Workloads** and **Individual components**:
- Desktop development with C++
- C++ Clang tools for Windows
- C++ CMake tools for Windows

> [!TIP]
> `build.bat` offers to install the Clang and CMake components for you if they're missing.

### 4. Build the Project
- Run `build.bat`. The first build recompiles the entire game and can take 30 to 90 minutes on weaker systems.
- When it finishes, run `out\build\release\SCDA.exe`.
