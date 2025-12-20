# Boshyst
IWBTB mod with hacks and tools to help creating TAS
## Building and running
For testing, just build the DLL with Visual Studio and inject into the game (make sure that [boshyst.conf](TODO) is in the game dir) <br />
For hourglass (Windows XP compatible), build the DLL using Visual Studio 2010 (manually create a project and add all the C++ files), put it into the game folder (with config file), replace wintasee.dll with [modified version](TODO) to automatically inject the mod. <br />
Building scripts: <br />
```sh
gcc start_cap.c -o start_cap.exe -mwindows
gcc end_cap.c -o end_cap.exe -mwindows
```
## Mouse bindings
TODO
## Recording
Use special [modified hourglass version](https://github.com/pixelsuft/hourglass-win32). <br />
Set allow_render to 1. <br />
FFmpeg is required to be in PATH variable. <br />
* [FFmpeg for modern Windows](https://www.gyan.dev/ffmpeg/builds/)
* [FFmpeg for 32-bit Windows 7](https://github.com/sudo-nautilus/FFmpeg-Builds-Win32/releases/tag/latest) (Use GPL builds)
* [FFmpeg for Windows XP](https://opensourcepack.blogspot.com/2017/12/ffmpeg-for-windows-xp.html)

Automatic start/end of the recording can be configured with render_start, render_count/render_end (not recommended because it mod injection is no stable between mod starts). <br />
It's recommended to set start/count vars to 0 and use start_cap and end_cap scripts. </ br>
When **not** using direct render, capture starts from the frame you currently see, and ends before the frame you currently see (front buffer capturing) <br />
When using direct render, capture starts from the frame you will see, and ends with the frame you currently see (back buffer capturing) <br />
