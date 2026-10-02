# Toolbar icons

The SVG sources are derived from Google Material Icons (`home`,
`arrow_back`, `center_focus_strong`, and `radio_button_checked`) and are licensed under
Apache License 2.0, matching the repository's root `LICENSE`. The source
modifications set the fill color to white for runtime color modulation and
make the SVG viewport responsive for exact-density rasterization.

The PNG files are generated from these SVG sources at 1x, 2x, and 3x density
so the SDL toolbar can select a pixel-matched asset without soft scaling.
The compact rounded-square recents glyph is rendered directly by the toolbar
at a supersampled resolution so its transparent bounds stay exact.

Source: https://github.com/google/material-design-icons
