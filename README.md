# Boshyst
IWBTB mod with hacks and tools to help speedrunning and creating TAS
## Cold start (mod menu)
Put [Boshyst DLL](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/boshyst.dll) and [DLL Injector](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/Inject-x86.exe) into the game folder <br />
Launch the game, open command prompt in game folder and type:
```sh
Inject-x86 boshyst.dll "I Wanna Be The Boshy.exe"
```
Menu should be openeded (toggled on Insert key by default).
## Cold start (BTAS) (experimental)
Just also put [BTAS launcher](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/btas.exe) into the game folder and launch the game via it. <br />
Open the config to see binds.
## Building and running
For testing, just build the DLL using Visual Studio and inject it into the game process (`boshyst.conf` config will be created is in the game dir). <br />
For hourglass (Windows XP compatible), build the DLL using Visual Studio 2010 (manually create a project and add all the C++ files, configure includes), put it into the game folder (with config file, also set `tas_mode` to 1 in config), replace `wintasee.dll` with [modified version](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/wintasee.dll) to automatically inject the mod when launching the game through the hourglass. Allow normal thread creation. <br />
Building helper scripts (use MinGW32 for compatibility):
```sh
gcc btas.c -o btas.exe
gcc start_cap.c -o start_cap.exe -mwindows
gcc end_cap.c -o end_cap.exe -mwindows
```
## Configuring
See `boshyst.conf` in the game folder
## Recording
Use special [modified hourglass r90 version](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/hourglass%20render.exe). <br />
Set `allow_render` to `1`. <br />
FFmpeg is required to be in `PATH` variable. <br />
* [FFmpeg for modern Windows](https://www.gyan.dev/ffmpeg/builds/)
* [FFmpeg for 32-bit Windows 7](https://github.com/sudo-nautilus/FFmpeg-Builds-Win32/releases/tag/latest) (Use GPL builds)
* [FFmpeg for Windows XP](https://opensourcepack.blogspot.com/2017/12/ffmpeg-for-windows-xp.html)

Automatic start/end of the recording can be configured with `render_start`, `render_count`/`render_end` (not recommended because mod injection is unstable between mod starts). <br />
It's recommended to set start/count vars to 0 and use [start_cap](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/start_cap.exe) and [end_cap](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/end_cap.exe) scripts. <br />
**Not** using direct render <=> front buffer capturing. <br />
Using direct render <=> back buffer capturing. <br />
Capture starts from the frame you will see, and ends with the frame you currently see <br />
Warning: game window should **not** be resized while recording.
## Useful links
*Latest Boshyst DLL is not available yet, build it yourself!
[Boshyst DLL (Windows 8+)](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/boshyst.dll)<br />
[Boshyst DLL (Windows XP compatible)](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/xp/boshyst.dll)<br />
[Modified wintasee.dll for hourglass r90](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/wintasee.dll) <br />
[Modified wintasee.dll for hourglass r78](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/wintasee%20r78.dll) <br />
Capture [start](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/start_cap.exe) and [end](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/end_cap.exe) scripts <br />
[BTAS launcher](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/btas.exe) <br />
[Simple DLL Injector](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/Inject-x86.exe) (Windows 7+) <br />
[Modified hourglass r90 for video recording](https://github.com/Pixelsuft/boshyst/raw/refs/heads/main/build/hourglass%20render.exe)
