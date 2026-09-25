# Changelog

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
