# GTA Native Resolution Patch

This plugin increases the internal resolution of the game to native 960x544 resolution and adds mipmap patches to use higher resolution textures. The trade-off is ~20FPS (originally ~30FPS) and 16bit color depth instead of 32bit. It is recommended to use this plugin in conjunction with [RemasteredControls](https://github.com/TheOfficialFloW/RemasteredControls) to get the greatest game experience.

<p align="center">
  <img src="https://github.com/TheOfficialFloW/GTANativeResolution/blob/master/screenshot.png">
</p>

## Donation

If you like my work and want to support future projects, you can make a donation:

- via bitcoin `361jRJtjppd2iyaAhBGjf9GUCWnunxtZ49`
- via [paypal](https://www.paypal.me/flowsupport/20)
- via [patreon](https://www.patreon.com/TheOfficialFloW)

## Requirements

- [Adrenaline v6.9](https://github.com/TheOfficialFloW/Adrenaline) or higher.
- Option `Force high memory layout` disabled (in `Recovery menu -> Advanced -> Advanced configuration`).

## Installation

- Download [gta_native.prx](https://github.com/TheOfficialFloW/GTANativeResolution/releases/download/v0.1/gta_native.prx).
- Copy it to `ux0:pspemu/seplugins/`.
- Write this line to `ux0:pspemu/seplugins/game.txt`:
  ```
  ms0:/seplugins/gta_native.prx 1
  ```

## Supported titles

- Grand Theft Auto: Vice City Stories [ULUS10160] (v3.00)
- Grand Theft Auto: Liberty City Stories [ULUS10041] (v3.00)

## Known bugs

- If enabled, other games may not be able to launch
- Not compatible with cheat plugins

## Credits

- Thanks to Kabuto_Kun for prior research.
