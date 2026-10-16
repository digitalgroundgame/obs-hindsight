# Installer branding

`installer-Windows.iss` uses these files when they are present, and Inno
Setup's default art when they are not. Use only official Digital Ground Game
artwork.

| File | Size (pixels) | Where it shows |
| ---- | ------------- | -------------- |
| `setup-icon.ico` | 16, 32, 48 and 256 in one .ico | The installer's own icon, and next to "Hindsight for OBS" in Settings > Apps |
| `wizard-image.bmp` | 164 × 314 | The tall banner on the installer's welcome and finished pages |
| `wizard-small-image.bmp` | 55 × 55 | The small logo in the top corner of the other pages |

Both images are 24-bit BMP files, which is the format Inno Setup 6 accepts.
For sharper art on high-DPI screens, add larger copies next to them, for
example `wizard-image-2x.bmp` (328 × 628) and `wizard-small-image-2x.bmp`
(110 × 110). The installer picks them up through the `*` wildcard and Windows
uses the best fit for the screen.

The current files come from the official square DGG logo (1024 × 1024).
The tall banners show it centered on its own blue (#1144FF).
