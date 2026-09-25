# Changelog

## 1.1.0

* **Resident Evil Requiem support** (requires REFramework).
* Works with right-handed engines such as RE Engine: the camera axis convention is now read from the
  game's projection, which fixes a doubled, smeared image during camera motion there. Unreal Engine
  games behave exactly as before.
* The add-on also finds the presenter next to the game's executable. This fixes "presenter not
  found" in games that load their DLLs from a staging folder (RE9's `_storage_`).
* Installer: double-click `install.bat` or `uninstall.bat` to choose the game from a numbered list of
  detected Steam games, or type a folder for non-Steam games.
* Installer: picks the live ReShade when a mod manager keeps backup copies. Uninstall also removes
  the add-on copies a game makes in its staging folder.
* The "presenter failed to start" message now shows the Windows error and the paths that were tried.

## 1.0.0

First release.

* Camera motion at the display's refresh rate, reprojected with NVIDIA Reflex 2 Frame Warp from the
  game's Streamline data.
* Camera prediction from the game's camera history and raw mouse input, with automatic calibration.
* Depth-correct parallax for camera movement (third-person orbit, walking), and a HUD that stays
  still.
* Auto latency at 1, 1/2 or 1/4 of a game frame, and frame pacing locked to the display.
* Safe when the DLSS preset (render resolution) changes during play.
* An installer that finds the game's ReShade, sets up Unreal Engine games, and uninstalls cleanly.
