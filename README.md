# BeatDrop Music Visualizer

BeatDrop is a stand-alone implementation of the amazing Milkdrop2 Winamp plug-in.

It lets you experience the stunning visual 2D effects with your music player of choice. No additional configuration steps needed! Just start BeatDrop and play your music.

Use BeatDrop with your favourite

* Music player:
  [foobar2000](https://www.foobar2000.org/),
  [VLC media player](https://www.videolan.org/vlc/index.html),
  [Clementine](https://www.clementine-player.org/),
  [AIMP](https://www.aimp.ru/),
  ...

* Web-based player:
  [SoundCloud](https://soundcloud.com/),
  [YouTube](https://www.youtube.com/),
  [Vimeo](https://vimeo.com/),
  ...

* Internet Radio station:
  [SomaFM](https://somafm.com/),
  [DI.FM](https://www.di.fm/),
  [RauteMusik.FM](https://www.rm.fm/),
  ...

## System Requirements
* Windows 10, Windows 8.1 or Windows 7 SP1

* WASAPI-compatible sound card

* Direct3D 9-compatible GPU and graphics driver

The build output includes the 32-bit `d3dcompiler_47.dll` runtime used to compile legacy Direct3D 9 shaders. Keep this DLL beside `BeatDrop.exe` when distributing the standalone build.

## Build on Windows
Install Visual Studio Build Tools with the C++ desktop workload and a Windows 10 SDK. Open the repository in VS Code and run `BeatDrop: Build Debug (Win32)` or `BeatDrop: Build Release (Win32)` from the Tasks menu. The Debug task is also the default build task. Each configuration folder (`Release` or `Debug`) has a `bin` subfolder containing `BeatDrop.exe`, `d3dcompiler_47.dll`, and the `data`, `docs`, `presets`, and `textures` resource folders, with intermediate build files kept in the `obj` subfolder.

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
