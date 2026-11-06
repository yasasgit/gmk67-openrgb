# OpenRGB plugin for the Zuoya GMK67

An [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) plugin that controls the lighting of the Zuoya GMK67
keyboard (USB `05AC:024F`) through its **stock firmware**. No flashing, no bootloader, and keymaps and macros are
never touched.

## Features

- **Direct mode:** live per-key colours on a 66-key layout, with OpenRGB's brightness slider. It uses the keyboard's
  real-time command, so OpenRGB effects animate without blinking (about 5 frames a second).
- **Save To Device:** in Direct mode, OpenRGB's Save To Device button stores the current per-key layout on the
  keyboard. Use it after Apply to make a static layout permanent (see Known limitations).
- **The keyboard's 19 built-in effects, plus Off:** colour or random colour, brightness, and speed or direction
  where the effect has them.
- **Limited writes:** one live frame every 50 ms at most (adjustable), and only when colours change. Nothing is saved
  on the keyboard unless you press Save To Device.
- **Cable detection:** notices immediately when the USB cable is unplugged or plugged in, shows Connected or
  Disconnected in **Information → GMK67**, and restores the current lighting after reconnecting.
- **Mode switching:** moves between the built-in effects and per-key Direct mode without the vendor app.

## Requirements

- **Windows 10/11** with the keyboard connected **by USB cable**. The 2.4 GHz dongle and Bluetooth don't accept
  these commands.
- **OpenRGB 1.1** (plugin API 5, Qt 6.8.3). The plugin must be built against the same OpenRGB commit and Qt version
  as your OpenRGB. See Help → About.
- **The vendor app must be closed** while OpenRGB is running.

## Building

You need:

- Visual Studio 2022 Build Tools with "Desktop development with C++" (includes CMake)
- Git
- Qt 6.8.3 for MSVC 64-bit:

  ```
  pip install aqtinstall
  aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O C:\Qt
  ```

Then run:

```
build.bat
```

`build.bat` does three things:

1. Downloads the OpenRGB source at the commit the plugin is built against, for its headers.
2. Builds `OpenRGBGMK67Plugin.dll`.
3. Copies the DLL to your Downloads folder.

Then copy the DLL to `%APPDATA%\OpenRGB\plugins`.

If Qt is installed somewhere else, run `set QT_DIR=<path>` first. To put the DLL somewhere other than Downloads,
run `set OUT_DIR=<folder>` first. For a different OpenRGB release, change the commit
in `build.bat` and the Qt version in `CMakeLists.txt`.

## Using it

1. Close the vendor app completely. It polls the keyboard and would interfere.
2. In OpenRGB, go to **Settings → Supported Devices** and untick **Keychron**. Its detector matches this VID/PID and
   sends a command that this keyboard treats as a macro write.
3. Restart OpenRGB. **Zuoya GMK67** appears as a keyboard device, and **Information → GMK67** shows the connection
   status.

### Frame rate

The live frame interval is in `%APPDATA%\OpenRGB\plugins\settings\ZuoyaGMK67.json`. The plugin creates this file on
first start:

```json
{ "frame_interval_ms": 50 }
```

The allowed range is 20-1000 ms, and the default is 50 ms. The vendor app uses about 185 ms. Restart OpenRGB after
changing it. If frames are dropped or the keyboard stops responding, increase it.

## Known limitations

- **In Direct mode, colours are live and temporary.** If no new frames arrive, the keyboard goes back to its stored
  per-key layout after a few seconds. Effects plugin animations aren't affected, because they keep sending frames.
  To keep a static layout, set your colours in Direct mode and press **Save To Device**. This writes them to the
  keyboard's stored layout, so they stay after OpenRGB closes and after unplugging.
- Switching into Direct mode also saves the current per-key colours once, as the vendor app does. Brightness changes
  and live frames don't save.
- The Scrolling direction values come from OpenRGB's direction numbers (left 0, right 1, up 2, down 3). They haven't
  been checked against the vendor app's labels.
- Custom Lighting (a different effect per key) and Music mode aren't implemented. OpenRGB's effects and audio
  visualiser plugins cover similar ground.

## Tools

These scripts need only Python 3 on Windows; nothing extra to install.

- `tools/gmk67_static.py`: standalone test script. `python gmk67_static.py 00 ff 00` sets solid green, and
  `python gmk67_static.py perkey` runs a per-key test.
- `tools/usbpcap_summary.py`: summarises a USBPcap `.pcapng` file without Wireshark.
  `python usbpcap_summary.py capture.pcapng ctrl` lists the HID class requests.

## Protocol

See [docs/PROTOCOL.md](docs/PROTOCOL.md).

## Credits

- [aloran-dev/gmk67-rgb](https://github.com/aloran-dev/gmk67-rgb) and
  [bitten-security/zuoya-gmk67](https://github.com/bitten-security/zuoya-gmk67) for earlier protocol work.
- The OpenRGB team for the plugin API.

## Disclaimer

Not affiliated with Zuoya. Use at your own risk. The plugin only sends lighting commands, which are also what the
vendor app sends when you change lighting.

## License

GPL-2.0-or-later, the same as OpenRGB, whose headers this plugin is built with. See [LICENSE](LICENSE).
