# X4 Todo (Xteink X4 Pro)

A small, open-source to-do list firmware for the **Xteink X4 Pro**, with a phone companion app over Bluetooth. Not affiliated with Xteink.

The X4 Pro shows your list and keeps it on screen even while asleep (e-ink needs no power to hold an image). Add, edit, reorder and delete tasks from your phone; tick them off by tapping them on the device or in the app.

## Status

First X4 Pro version. It has **not been compiled or run on hardware yet**. The hardware drivers come from the FreeInk SDK, whose X4 Pro support was confirmed on real devices, so the main unknowns are screen orientation and touch direction (see "First-run checklist").

## Using it

| Control | What it does |
| --- | --- |
| Tap a task | Mark it done / not done |
| Tap "Remove done", then tap again | Remove all finished tasks |
| Tap `<` / `>`, or the Left / Right buttons | Previous / next page |
| Home key | Back to page 1, and fully refresh the screen |
| Hold Home key | Light: low, medium, high, off (remembered) |
| Power | Sleep (the list stays on screen). Press again to wake |

The device sleeps after 3 minutes without activity (never while your phone is connected). Bluetooth only works while it's awake.

## Before anything else: back up

Use xteink.dve.al → "Save full flash" and keep the 16 MB file somewhere safe. To go back to the stock firmware later, use the same site's "Write full flash from file" with that backup.

## Building and flashing (step by step)

You'll do this once to install the firmware, and again whenever the firmware changes.

1. **Install VS Code** from code.visualstudio.com.
2. **Install PlatformIO:** in VS Code, click the Extensions icon on the left (four squares), search for "PlatformIO IDE", and click Install. Wait for it to finish, then restart VS Code if it asks.
3. **Open this project:** unzip it, then in VS Code use File → Open Folder and choose the `x4pro-todo` folder (the one containing `platformio.ini`).
4. **Build:** click the checkmark icon in the blue bar at the bottom of the window. The first build downloads the ESP32 tools and takes several minutes. It ends with `SUCCESS` or with error messages. If you see errors, copy them to Claude.
5. **Plug in the X4 Pro:** wake it, attach the magnetic adapter, and connect the cable to your computer.
6. **Flash:** hold the X4 Pro's **Power button**, click the right-arrow icon (Upload) in the bottom bar, and **keep holding Power** until the output says the upload finished and the device restarts. Then let go.
7. **Check the screen:** you should see "To do" and "Nothing to do." If the screen stays blank, press Power once to wake it.

If the device shows the stock software again after a restart, tell Claude: the firmware may not have confirmed its start-up (the stock bootloader rolls back unconfirmed firmware).

## First-run checklist

- **Screen upside down?** In `src/config.h`, change `DISPLAY_ROTATION = 3` to `1`, then build and flash again.
- **Taps land in the wrong place?** Tap near a corner and see where it registers. Then set `TOUCH_FLIP_X` and/or `TOUCH_FLIP_Y` to `true` in `src/config.h`.
- **Battery shows nothing?** It's read from the X4 Pro's battery chip; it can take a moment after first boot.

## Companion app

`app/index.html` is a single-file web app that talks to the X4 Pro over Bluetooth. Browsers only allow Bluetooth on secure (https) pages, so it needs hosting. The free option is GitHub Pages; Claude can walk you through it.

Works in Chrome on Android, and Chrome/Edge on a computer. iPhone Safari doesn't support Web Bluetooth; use the free Bluefy browser there.

## Security note

There's no Bluetooth pairing yet: anyone nearby could connect while the device is awake. Passkey pairing (showing a code on the e-ink screen) is the planned next step.

## Bluetooth protocol

Service `7d2a0001-5b1e-4c8e-9a51-2f3a8c9e0b01`

- Command characteristic `...0002` (write), one JSON object per write: `{"op":"get"}`, `{"op":"add","text":"..."}`, `{"op":"toggle","id":3}`, `{"op":"del","id":3}`, `{"op":"edit","id":3,"text":"..."}`, `{"op":"move","id":3,"to":0}`, `{"op":"clear"}`.
- State characteristic `...0003` (notify): the full list as `{"tasks":[{"id":1,"done":false,"text":"..."}],"max":40}`, split across notifications and ending with `\n`.

Limits: 40 tasks, 120 characters each, plain ASCII text (other characters show as `?`).

## Project layout

```
platformio.ini    build settings
partitions.csv    flash layout (same as stock)
src/config.h      settings: sleep timer, orientation, touch direction
src/tasks.*       task list, saved to flash as JSON
src/ui.*          screen drawing and tap targets
src/ble_link.*    Bluetooth service
src/main.cpp      glue: touch, buttons, commands, sleep
app/index.html    phone companion app
include/Logging.h small helper the SDK expects
freeink-sdk/      X4 Pro hardware drivers (FreeInk SDK, MIT, see VENDORED.txt)
```

## License

MIT (see LICENSE). The bundled FreeInk SDK is MIT-licensed; see `freeink-sdk/LICENSE` and `freeink-sdk/NOTICE`.
