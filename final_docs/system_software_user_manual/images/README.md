# SSUM screenshot slots

Drop screenshots into this directory at the exact filenames below.
The SSUM `.tex` source uses `\IfFileExists{}` for each one, so the
PDF builds with a gray "screenshot pending" placeholder when a file
is missing and automatically embeds the real image on the next
rebuild.

## Required screenshots

| Filename | What to capture | Aspect | Where it lands in the PDF |
|---|---|---|---|
| `grafana-cockpit.png` | Full STAR Grafana dashboard at default zoom -- map panel, motor gauges, autonomy / e-stop / speed / SLAM controls, IMU and scan panels all visible. Browser chrome cropped out. | Wide (~16:9) | Section 6.1 "Grafana Cockpit" |
| `grafana-controls-zoom.png` | Close-up of the four control buttons + speed slider (autonomy toggle, e-stop, SLAM start, SLAM stop, speed slider) so a reader can see exactly which widget is which. | Wide or square | Section 5.5-5.7 (cockpit operation) |
| `lichtblick-3d.png` | Lichtblick 3D panel with the robot on a partially-mapped scene: live `/scan` (green LiDAR rays), `/map` occupancy grid, `/cloud_map` colored 3D points, and the `/odometry/filtered` arrow visible. Side panels with IMU plots optional. | Wide | Section 6.2 "Lichtblick Layout" |
| `lichtblick-connection.png` | Lichtblick "Open Connection" dialog with `ws://100.64.0.6:8765` filled into the WebSocket field. | Tall or square | Section 4.7 "Verifying the Install" |

## Capture tips

- **Browser:** Chromium-based browser, F11 fullscreen, then the OS
  screenshot tool gives the cleanest crop.
- **Resolution:** target at least 1600 px wide so the PDF stays
  crisp at print size. Larger is fine; LaTeX scales down.
- **Format:** PNG strongly preferred for UI screenshots
  (lossless, sharp text). JPEG for photographs only.
- **No personal info:** crop out browser address bars, tab names,
  and the operator name in the Grafana top-right user menu before
  saving. The Tailscale IP `100.64.0.6` is fine to show -- it is
  documented elsewhere in the SSUM.
- **Reset Grafana time range to "Last 5 minutes"** before capturing
  so panels display live data, not flatlined history.

## After dropping a file in

```
cd final_docs/system_software_user_manual
pdflatex -interaction=nonstopmode STAR_System_and_Software_User_Manual.tex
pdflatex -interaction=nonstopmode STAR_System_and_Software_User_Manual.tex
```

(Two passes so the table of contents and figure list resolve.)
The new PDF will embed every PNG that has been added; remaining
slots stay as placeholders.

## Style note

Until you take real screenshots, the placeholders show a labelled
gray box with the screenshot's intended caption. This is the
intended fallback -- the PDF is always reviewable, it just gets
better as screenshots land.
