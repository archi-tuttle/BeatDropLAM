# BeatDropLAM ("Lean and Mean") Fork

Thanks to Maxim Volskiy (and co-authors), the author of the original [BeatDrop-project](https://github.com/mvsoft74/BeatDrop), this project finished for me a nearly two-decade long desperate search for a suitable container for the legendary MilkDrop-Plugin, making it independent of Winamp. And sadly I didn't find it already in 2018, when Maxim published it.

Even though Winamp was a revolutionary music player at its time, UX paradigms have shifted and usage scenarios have vastly extended. Nowadays, we have a huge variety of possible sources, which simply cannot be covered by a single player. We play music on YouTube, stream from Spotify, use a web radio player or stream from [Music Assistant](https://www.music-assistant.io/) locally to a [Squeezelite](https://sourceforge.net/projects/lmsclients/files/squeezelite/windows/) client on the PC.

Maxim Volskiy's BeatDrop is superior in that it provides an extremely lightweight UI around Milkdrop and WASAPI loopback ("play what you hear on your PC") and hides all the most basic configuration stuff behind the help menu. One unique selling proposition, which I highly appreciate, is the lack of those "VJ features" I found exceptionally distracting with other MilkDrop wrappers (like MilkDrop 3). Come on, I am not gonna use MilkDrop at Tomorrowland, are you serious? I just need a single-click visualization, period.

Nevertheless, I have decided to give it a tiny amount of polish (primarily for my own purposes, I admit). Usually such minor changes wouldn't justify a fork, but the original author doesn't seem to respond to pull requests. If you, Maxim, ever react to this, I would offer to merge the changes into your repo.

Since I wanted BeatDropLAM ("Lean and Mean") to be easily remote-controllable from [Home Assistant](https://www.home-assistant.io/) (namely through [HASS-Agent](https://www.hass-agent.io/) on the PC that is running BeatDrop), I have blocked the bordered window mode altogether. Personally, I have always thought that showing MilkDrop in a window is pointless anyway, it's like keeping a Ferrari in a showroom. Besides that, needing to enter fullscreen mode (or borderless windowed mode) through a toggle button is awkward for remote-control: if the app already runs in fullscreen mode, a remote request for a toggle to fullscreen will end up in a bordered window instead, without the controller noticing this. So BeatDropLAM now always starts up in fullscreen mode and stays there. In order to mitigate the foreseeable UX issues this behaviour might cause, I have taken care that the app can be exited easily by a multitude of keys that are standard for that purpose (see help menu for the comprehensive list). Finally I have put the help menu on the standard F1 key (like other forks have already done) and removed any winamp references from the help menu.

Long story short, this is the summary of the "features" I added:
* runs fullscreen only
* exit visualization/app by Alt+F4 / ESC / Q / X / F11 / RMB (Right Mouse Button) keys
* preset change also by left and right cursor keys
* cleaned up help menu and put it on F1
* some minor shortcut improvements
* increased blend times for a more harmonic visual experience (not a real burden for current GPUs)
* switched to 2026 Windows 11/10 SDK and MSVC toolchain
* removed DirectX SDK+DLLs (which is now already included in Windows SDK)

I have also ported the project structure to be easily used and built from inside VSCode, using the current (2026) MSVC build tools. Most of that bloody work has been done by GitHub Copilot, because I loathe that files stuff. I am just a casual developer, and I am doing something completely different in my regular job, so please forgive me if I had introduced some "unorthodox" things in the process.

The remaining sections are a blend of Maxim's original readme and some accounts of the changes I have made.

# BeatDrop Music Visualizer

BeatDrop is a stand-alone implementation of the amazing Milkdrop2 Winamp plug-in.

It lets you experience the stunning visual 2D effects with your music player of choice. No additional configuration steps needed! Just start BeatDrop and play your music.

Use BeatDrop with any your favourite desktop music players, web-based player/radio/streaming clients or local streaming clients (e.g. squeezelite).

## System Requirements
* Windows 11, Windows 10, Windows 8.1 or Windows 7 SP1

* WASAPI-compatible sound card

* Direct3D 9-compatible GPU and graphics driver

The build output includes the 32-bit `d3dcompiler_47.dll` runtime used to compile legacy Direct3D 9 shaders. Keep this DLL beside `BeatDrop.exe` when distributing the standalone build.

## Build on Windows
Install Visual Studio Build Tools with the C++ desktop workload and a Windows 11/10 SDK. Open the repository in VS Code and run `BeatDrop: Build Debug (Win32)` or `BeatDrop: Build Release (Win32)` from the Tasks menu. The Debug task is also the default build task. Each configuration folder (`Release` or `Debug`) has a `bin` subfolder containing `BeatDrop.exe`, `d3dcompiler_47.dll`, and the `data`, `docs`, `presets`, and `textures` resource folders, with intermediate build files kept in the `obj` subfolder.

## Acknowledgements
Special thanks to:

* Ryan Geiss and Rovastar (John Baker) [official Milkdrop2 source code](https://sourceforge.net/projects/milkdrop2/)

* oO-MrC-Oo [XBMC plugin](https://github.com/oO-MrC-Oo/Milkdrop2-XBMC)

* Casey Langen [milkdrop2-musikcube](https://github.com/clangen/milkdrop2-musikcube)

* Matthew van Eerde [loopback-capture](https://github.com/mvaneerde/blog)

* and all the preset authors!

## License

[license]: #license

This repository is licensed under the 3-Clause BSD License ([LICENSE](LICENSE) or [https://opensource.org/licenses/BSD-3-Clause](https://opensource.org/licenses/BSD-3-Clause)) with the exception of where otherwise noted.

Although the original Matthew van Eerde's [loopback-capture](https://github.com/mvaneerde/blog) project didn't explicitly state the license, the author has been kind enough to provide the [license clarification](
https://blogs.msdn.microsoft.com/matthew_van_eerde/2014/11/05/draining-the-wasapi-capture-buffer-fully/)

> ### Sunny March 29, 2015 at 11:06 pm
> Hi. Just wondering is this open source? I'm looking for something like this for my school project.
>
> ### Maurits [MSFT] March 30, 2015 at 8:35 am
> @Sunny do with the source as you like.

All changes in this repository to the original Matthew's code are published either under the terms of BSD license or the license provided by original author.

## Contributions

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, shall be licensed as above.
