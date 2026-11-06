# Zuoya GMK67 lighting protocol (stock firmware)

Notes on the lighting protocol of the Zuoya GMK67 (USB `05AC:024F`, HFD801KJC MCU), from USB captures of the
vendor app (`ZUOYO GMK67 Driver` 1.0.1.1) and from its language and settings files. Wired USB only.

Status labels: **verified** means it was tested on the keyboard with this plugin. **Inferred** means it was seen in
captures or vendor files but not tested on its own.

## Transport

- Interface `MI_00`. This is the HID collection whose feature report length is 65 (64 data bytes, no report ID).
- Windows only lets this collection be opened with access `0`, not read/write. Feature reports still work.
- Each command is a `SET_REPORT (Feature)` followed by a `GET_REPORT (Feature)`. The vendor app reads the reply
  back. This plugin reads it too and ignores the contents.
- Each command shown below is padded with zeros to 64 bytes.

## Transactions

Every write is wrapped in a begin/end pair:

```
04 18                          begin
...                            command + data pages
04 f0                          end / apply
```

## Lighting effect (`04 13`, 1 page), verified

```
04 18
04 13 00 00 00 00 00 00 01     one page follows
<effect page>
04 f0
```

Effect page (first 16 bytes; the rest of the 64 bytes is zero):

| Offset | Meaning |
|---|---|
| 0 | Effect id (table below). `0x80` = per-key (custom) mode |
| 1-3 | R, G, B |
| 4-7 | `00` |
| 8 | Random colour: `1` = random, `0` = use RGB |
| 9 | Brightness + 1 (`01`-`10`) |
| 10 | Speed + 1 (`01`-`10`) |
| 11 | Direction (only Scrolling; see below) |
| 12-13 | `00` |
| 14-15 | `aa 55` |

Effects, in the order of language ids 200-219 and confirmed byte by byte in a capture of all 20:

| Id | Name | Random | Speed byte sent by the vendor app |
|---|---|---|---|
| `01` | Static | no | `0b` |
| `02` | SingleOn | no | fixed `06` |
| `03` | SingleOff | no | fixed `06` |
| `04` | Glittering | yes | user setting |
| `05` | Falling | yes | user setting |
| `06` | Colourful | yes | fixed `10` |
| `07` | Breath | yes | user setting |
| `08` | Spectrum | yes | fixed `01` |
| `09` | Outward | yes | user setting |
| `0a` | Scrolling | yes | fixed `03`, direction default `02` |
| `0b` | Rolling | yes | user setting |
| `0c` | Rotating | yes | user setting |
| `0d` | Explode | yes | user setting |
| `0e` | Launch | yes | fixed `01` |
| `0f` | Ripples | yes | fixed `01` |
| `10` | Flowing | yes | user setting |
| `11` | Pulsating | yes | user setting |
| `12` | Tilt | yes | user setting |
| `13` | Shuttle | yes | user setting |
| `00` | LED Off | no | `01` (brightness `01`) |

"Fixed" means the vendor app always sent this value for that effect, whatever its saved speed. That suggests the
effect has no speed setting (inferred).

## Per-key colours (`04 23`, 9 pages), verified

```
04 18
04 23 00 00 00 00 00 00 09     nine pages follow
<9 x 64-byte pages = 576 bytes>
04 f0
```

- **Layout:** key `light_index` N occupies bytes `N*4 .. N*4+3` as `[N, R, G, B]`. The last two bytes of the
  576-byte area are `aa 55`.
- **Showing the colours:** select effect `0x80` with the `04 13` command.

### Switching to per-key mode

There is no separate mode-switch command. When the vendor app switches to **Key light customization**, it sends the
per-key save and then the `0x80` effect page:

```
04 18, 04 23 ...[8]=09, <9 pages>, 04 f0
04 18, 04 13 ...[8]=01, 80 ff 00 00 00 00 00 00 01 0f 0f 00 00 00 aa 55, 04 f0
```

On the `0x80` page, byte 8 is `01` and byte 10 is `0f`. Switching back to an effect is just that effect's page.
Custom Lighting and Music send only live `04 20` frames, with no mode command.

`light_index` values, from the vendor `KeyboardLayout.xml`:

| Row | Keys → light_index |
|---|---|
| 1 | Esc 1, `1`-`=` 20-31, Backspace 103 |
| 2 | Tab 37, Q-`]` 38-49, `\` 67, Del 119 |
| 3 | Caps 55, A-`'` 56-66, Enter 85, PgUp 118 |
| 4 | LShift 73, Z-`/` 74-83, RShift 84, Up 101, PgDn 121 |
| 5 | LCtrl 91, Win 92, LAlt 93, Space 94, RAlt 95, Fn 96, Left 99, Down 100, Right 102 |

Indices 97 and 98 are the status LEDs next to Fn. They can't be set.

## Live per-key frames (`04 20`, 8 pages), from vendor Music mode

This is the vendor app's real-time animation command. It has no `04 18` / `04 f0` wrapper and nothing is saved, so
it doesn't blink the way repeated `04 23` saves do.

```
04 02                          (reply read)
04 20 00 00 00 00 00 00 08     eight pages follow (reply read)
<8 x 64-byte pages = 512 bytes, no replies read>
```

- **Frame layout:** a **packed** list of `[light_index, R, G, B]` entries in row order (Esc, `1` … `=`, Backspace,
  Tab …), 66 keys × 4 = 264 bytes. The rest is zero, with no `aa 55` ending.
- **Colour range:** the vendor app's colour values never go above `fe`.
- **Rate:** the vendor app sends one frame about every 185 ms, with pages about 8 ms apart. The plugin uses the same
  rate.
- **Persistence:** live colours are temporary. If frames stop, the keyboard goes back to its stored lighting after a
  few seconds, which is why the vendor app keeps resending even an unchanged frame. To keep a layout, save it with
  `04 23`.

## Do not send

These overwrite non-lighting settings. This plugin never sends them.

| Command | Effect |
|---|---|
| `04 11` + 9 pages | Replaces the whole base-layer keymap |
| `04 27` + 9 pages | Replaces the whole Fn-layer keymap |
| `04 19`, `04 15`, `04 02` | Macro write. OpenRGB's Keychron controller sends `04 19`; disable it for this keyboard. |

The vendor app also ships a firmware updater (`米乐068_67键_三模_RGB_V1.00_HFD801KJC_...exe`). Don't run it to
experiment.

## Background polling

While open, the vendor app repeatedly sends `04 02` and `04 f5 ... [8]=09`. Close the vendor app while using
OpenRGB.

## Vendor app modes not yet replicated

Their settings structures come from the vendor app's SQLite database (`db\RKGK890_data.db` in
`Documents\ZUOYO GMK67 Driver Files`). Their wire format is unknown.

- **Custom Lighting:** each key has its own effect (`light_mode` 1, 4-12), colour type and colour, plus a speed and
  brightness for the whole scheme. The config key `customlight_run` suggests the vendor app animates it on the PC.
- **Custom Lighting Mode:** saved per-key static colour schemes. OpenRGB profiles cover this.
- **Music:** an audio visualiser run by the vendor app. OpenRGB's audio visualiser plugin covers this.
- **Light sleep:** fixed in the firmware (backlight off after 5 minutes idle; Bluetooth deep sleep after 30 minutes),
  according to the keyboard manual. There is no setting to send.

## Sources

- Our own USBPcap captures.
- [aloran-dev/gmk67-rgb](https://github.com/aloran-dev/gmk67-rgb): effect page layout.
- [bitten-security/zuoya-gmk67](https://github.com/bitten-security/zuoya-gmk67) `docs/PROTOCOL.md`: per-key and
  keymap/macro commands.
