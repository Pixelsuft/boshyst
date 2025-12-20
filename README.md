# Boshyst
IWBTB mod with hacks and tools to help creating TAS
## Building and running
For testing, just build the DLL with Visual Studio and inject it into the game (make sure that [boshyst.conf](https://github.com/Pixelsuft/boshyst/blob/main/boshyst.conf) is in the game dir) <br />
For hourglass (Windows XP compatible), build the DLL using Visual Studio 2010 (manually create a project and add all the C++ files, configure includes), put it into the game folder (with config file), replace wintasee.dll with [modified version](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/wintasee.dll) to automatically inject the mod when launching through the hourglass. Allow normal thread creation. <br />
Building recording helper scripts:
```sh
gcc start_cap.c -o start_cap.exe -mwindows
gcc end_cap.c -o end_cap.exe -mwindows
```
## Configuring
See [boshyst.conf](https://github.com/Pixelsuft/boshyst/blob/main/boshyst.conf)
## Recording
Use special [modified hourglass version](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/hourglass%20render.exe). <br />
Set allow_render to 1. <br />
FFmpeg is required to be in PATH variable. <br />
* [FFmpeg for modern Windows](https://www.gyan.dev/ffmpeg/builds/)
* [FFmpeg for 32-bit Windows 7](https://github.com/sudo-nautilus/FFmpeg-Builds-Win32/releases/tag/latest) (Use GPL builds)
* [FFmpeg for Windows XP](https://opensourcepack.blogspot.com/2017/12/ffmpeg-for-windows-xp.html)

Automatic start/end of the recording can be configured with render_start, render_count/render_end (not recommended because mod injection is unstable between mod starts). <br />
It's recommended to set start/count vars to 0 and use start_cap and end_cap scripts. <br />
When **not** using direct render, capture starts from the frame you currently see, and ends before the frame you currently see (front buffer capturing) <br />
When using direct render, capture starts from the frame you will see, and ends with the frame you currently see (back buffer capturing) <br />
## Useful links
[Boshyst DLL](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/boshyst.dll) <br />
[Modified wintasee.dll for hourglass r90](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/wintasee.dll) <br />
[Modified wintasee.dll for hourglass r78](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/wintasee%20r78.dll) <br />
Capture [start](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/start_cap.exe) and [end](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/end_cap.exe) scripts <br />
[Simple DLL Injector](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/Inject-x86.exe) (Windows 7+) <br />
[Modified Hourglass r90 for recording](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/hourglass%20render.exe)
