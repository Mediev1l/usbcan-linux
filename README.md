# usbcan-linux

A CAN bus monitor for Linux: a modern replacement for the Waveshare **USBCAN V2**
Windows tool. Receive, filter, decode, save and send CAN frames.

C++20 · CMake · Qt 6 / QML · 100 % unit test coverage of the application logic.

- **Adapters**
  - **Waveshare USB-CAN-A** (CH340, `1a86:7523`) talked to directly over its serial
    port. No kernel driver, no SocketCAN bridge.
  - **Any SocketCAN interface:** `can0`, `vcan0`, PEAK, candleLight, …
- **Trace:** chronological or *latest per ID*. Changed bytes are highlighted, and each
  ID shows its frame count and period.
- **Expandable frames:** click a frame to see the J1939 decode (PGN and name, priority,
  source/destination address, PDU1/PDU2), the identifier bits and every byte in
  hex/dec/bin.
- **Filters:**
  - Every ID seen is added to a checklist automatically. Untick it to hide that ID,
    double-click to show only that ID.
  - "Show new IDs" decides whether newly seen IDs start ticked.
  - Hidden IDs are remembered between sessions.
- **Saved messages:** a name, ID, payload, optional period and comment. Save from the
  trace or the filter list (right-click), or type a new one. Send once or periodically.
  Saved names appear next to matching frames in the trace.
- **Files:** export the trace to CSV, candump or Vector ASC. Import CSV and candump.
  Saved messages are stored as JSON.

## Install

On Ubuntu 24.04:

```bash
git clone <this repo> && cd usbcan-linux
scripts/install.sh deps       # build dependencies (asks for sudo)
scripts/install.sh            # Release build, installed to ~/.local
```

Then start **usbcan-linux** from the application menu, or run `~/.local/bin/usbcan-linux`.

- Install elsewhere: `PREFIX=/opt/usbcan-linux scripts/install.sh`
- Remove: `scripts/install.sh uninstall` (saved messages and settings are kept)
- Update: `git pull && scripts/install.sh`

The Waveshare adapter needs access to the serial port:

```bash
sudo usermod -aG dialout $USER     # then log out and back in
```

## Build for development

```bash
cmake --preset debug               # Debug build with tests
cmake --build --preset debug
ctest --preset debug               # all unit and GUI tests
./build/debug/src/app/usbcan-linux
```

| Preset | Purpose |
|---|---|
| `debug` | Development build, tests included |
| `release` | Optimised build, no tests |
| `coverage` | Instrumented build; `cmake --build --preset coverage --target coverage` runs every test and **fails below 100 %** line/function coverage. HTML report: `build/coverage/coverage/index.html` |

Requirements: CMake ≥ 3.21, a C++20 compiler (GCC 13 tested), Qt ≥ 6.4 (Core,
SerialPort, Qml, Quick, QuickControls2, QuickTest), GoogleTest/GMock, Ninja. The coverage
target also needs [gcovr](https://gcovr.com) (`pipx install gcovr`).

## Usage

### Connecting

1. Choose **Waveshare USB-CAN-A** or **SocketCAN** in the top bar.
2. **Waveshare settings:**
   - **Port:** the stable `/dev/serial/by-id/usb-1a86_…` path is listed first, so
     re-plugging the adapter or a dock does not break anything.
   - **Bitrate, mode and frame type:** the same settings as in the vendor tool.
     The serial link always runs at 2 Mbaud with the "variable length" protocol and
     automatic retransmission on.
3. Press **Connect**. If the adapter is unplugged, the app shows an error instead of
   hanging, and you can simply connect again.

| Mode | Use |
|---|---|
| Normal | Default. The adapter acknowledges frames, which is needed when it and your device are the only nodes on the bus. |
| Silent | Listen only, never transmits (not even ACKs). Only sees frames if another node acknowledges them. |
| Loopback / Loopback + silent | Adapter self-test. |

Only one program can use the adapter at a time, so close the vendor tool, `wscan-bridge`
and similar first. The adapter supports classic CAN only, not CAN FD.

### Trace

- **Chronological / Latest per ID:** a full history, or one row per identifier.
- **Click a row** to expand the decode. **Right-click a row** for:
  - *Save to library…*
  - *Send again*
  - *Copy as cansend*
  - *Hide this ID*
- **Pause** stops updating the view (frames are still counted). **Clear** empties the
  trace.
- **Search** matches ID, name, PGN, PGN name and data bytes.

### Filters (left panel)

- Untick an ID to hide it, double-click to show only it.
- **All / None / Invert** change all checkboxes at once.
- **Forget** clears the list and unhides everything. IDs still on the bus come back as
  their next frame arrives; IDs that went quiet disappear.
- **Show new IDs** off: IDs that appear from now on start hidden. Useful for spotting
  new traffic.
- **RX / TX** show or hide received and sent frames.
- **Right-click an ID** → *Save last frame to library…*

### Sending (right panel)

- **New frame:** type the ID and data, then press **Send**, or **Save as…** to keep it.
  29-bit is ticked automatically for IDs longer than 3 digits.
- **Saved messages:**
  - **Send** sends the message once.
  - **Every N ms** starts periodic sending; press it again to stop it.
  - **■ Stop all** stops every periodic job.
  - **⋮** lets you edit, reorder or delete a message. Edits apply immediately, even
    while the message is being sent.

## File formats

### Trace: *File → Export trace…*

The format follows the file extension:

| Extension | Format | Import | Use it for |
|---|---|---|---|
| `.csv` | `time_s,direction,id,extended,rtr,dlc,data,name` | yes | Spreadsheets. The only format that includes saved names. |
| `.log` / `.candump` | candump `-l`: `(1760376017.337000) can0 18FF9341#32FF…` | yes | can-utils, e.g. `canplayer -I trace.log` to replay onto a vcan |
| `.asc` | Vector ASCII log | no | CANalyzer, CANoe and other Vector tools |

Export writes the whole trace buffer (up to the last 200 000 frames), regardless of
filters and view mode. Timestamps are absolute Unix time; ASC uses time relative to the
first frame.

### Saved messages: *File → Export / Import saved messages…*

Saved messages are stored automatically on every change, in
`~/.local/share/usbcan-linux/usbcan-linux/library.canlib.json`. Export/import is for
backups, sharing, or keeping separate sets per device. **Import replaces** the current
list.

```json
{
  "format": "usbcan-library",
  "version": 1,
  "messages": [
    {
      "name": "Display status",
      "id": "18FF9341",
      "extended": true,
      "payload": "32 FF FF FF FF FF 0F FF",
      "periodMs": 0,
      "comment": "seen on the bench"
    }
  ]
}
```

- `id` is hex.
- `payload` takes hex bytes with or without spaces.
- `periodMs` set to `0` means send once.

The file is plain text, so it is easy to edit by hand or keep in git.

Settings (window layout, last adapter, hidden IDs) are stored in
`~/.config/usbcan-linux/usbcan-linux.conf`.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `Permission denied` when connecting | Add yourself to `dialout` (see Install) and log in again. |
| The port disappears right after plugging in | `brltty` may grab CH340 devices: `sudo apt remove brltty` if you do not use a braille display. |
| Connected but no frames | Check bitrate (e.g. 250 kbit/s), wiring and termination (120 Ω). In *Silent* mode nothing is received unless another node ACKs; use *Normal*. |
| `Cannot open … Device or resource busy` | Another program (vendor tool via VM, `wscan-bridge`, …) holds the port. |
| SocketCAN interface missing from the list | Bring it up first: `sudo ip link set can0 up type can bitrate 250000`, then press ⟳. |

## Project layout

```
src/core      protocol, J1939, transports, trace/filter models (Qt Core only)
src/backend   AppController: everything the QML UI binds to
qml           the UI (QML module "UsbCan", Dracula colours in Theme.qml)
src/app       main()
tests         core / backend (GoogleTest) and qml (Qt Quick Test), shared fakes
scripts       install.sh, render-icons.cpp (regenerates resources/icons/*.png)
```

All logic lives in `src/core` and `src/backend` and is unit tested to 100 % line and
function coverage. QML is presentation only and is covered by GUI behaviour tests
running against a fake adapter. Hardware is never touched by the tests: the only
real-syscall test uses the loopback interface and never opens a CAN interface.
