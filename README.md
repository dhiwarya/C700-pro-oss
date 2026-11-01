# C700-pro-oss — open firmware for the GEOID CC700 Pro

Open-source firmware for the **GEOID CC700 Pro** bike computer, with a
redesigned "Daylight" interface. It is a fork of
**[c606-oss](https://github.com/HighwayStar/c606-oss)** by Vitalii Tomin
(HighwayStar) and contributors, which does all the hard work: the
reverse-engineered hardware support, ride recording, maps, routes and ANT+
sensors. This fork adds a new look and a desktop screen renderer on top.

> **Not affiliated with Magene, GEOID or Onelap.** Flashing third-party
> firmware is at your own risk. Like c606-oss, it only replaces the app
> slot: the vendor bootloader, settings and coprocessor firmware stay as
> they are, and the vendor firmware can be booted again at any time.

| Home | Ride | Map | Paused |
|:--:|:--:|:--:|:--:|
| ![home](docs/images/daylight_home.png) | ![ride page](docs/images/daylight_ride_1.png) | ![map](docs/images/daylight_map.png) | ![paused](docs/images/daylight_ride_paused.png) |

*Rendered from the firmware code by `tools/uisim` (a test loop around
Gelora Bung Karno, Jakarta). Verified on a CC700 Pro.*

## What this fork changes

* **Daylight UI** ([docs/UI_DESIGN.md](docs/UI_DESIGN.md)): a status bar on
  every page with a chip that names what key 0 does next, the ride state,
  GPS, battery and settings; a home page with a ready-to-ride checklist
  (GPS, route, sensors, sunset) and Menu / Start buttons; data cells with
  small captions and large condensed numbers sized to fit; a soft-key bar
  labelling the bottom keys; a heading arrow, north marker and larger
  controls on the map. Light theme by default (the CC700 Pro's screen is
  transflective, so light stays readable in sunlight); the dark "Night"
  theme follows the same layout.
* **New layouts** `5C` (ride page) and `M3` (three-field map strip) with
  matching defaults. On the home page key 1 opens the menu.
* **`tools/uisim`**: the real UI, settings and map renderer compiled for the
  desktop with stand-ins for the hardware, rendering every screen to PNG, so
  UI work does not need a flash per change.
* **Fonts**: Barlow Semi Condensed and Montserrat, generated for LVGL by
  `tools/gen_fonts.sh`.

Everything else (building, flashing, the hardware notes) is upstream's and
documented below and in [docs/](docs/). Build for this device with
`BOARD=cc700pro`.

## Credits and licence

* Based on **c606-oss** — <https://github.com/HighwayStar/c606-oss>, forked
  at commit `30c7aee` (2026-10-01). Original work © Vitalii Tomin
  (HighwayStar) and contributors (Marius Schiffer, Giovanni Harting).
  Please credit and support the upstream project.
* Licensed **GPL-3.0-or-later**, the same as upstream ([LICENSE](LICENSE)).
  The files this fork changed, and third-party licences (LVGL, ESP-IDF,
  TinyUSB, the fonts), are listed in [NOTICE](NOTICE).
* To cite this work, see [CITATION.cff](CITATION.cff).

---

*The rest of this README is the upstream c606-oss README.*

## c606-oss — open firmware for Magene bike computers

An open-source replacement firmware for the **Magene C606** GPS bike
computer, also running on the **C706** and the **Geoid CC700 Pro**. It
records rides as standard FIT files, shows configurable data pages and
offline vector maps, follows GPX routes and talks to ANT+ sensors — and
it installs next to the vendor firmware, which you can put back at any
time.

> **Status: proof of concept.** It is used on real devices, but expect
> rough edges. Flashing only replaces the app slot; the vendor
> bootloader, settings, coprocessor firmware and your data stay as they
> are.

| Idle | Data page | Map | Ride summary |
|:--:|:--:|:--:|:--:|
| ![idle](docs/images/idle_dark.png) | ![data page](docs/images/page3.png) | ![map](docs/images/map_dark.png) | ![summary](docs/images/summary.png) |

More in the [screenshot gallery](docs/SCREENSHOTS.md).

## Features

* **Ride recording** to FIT activity files — Strava, Garmin Connect, Golden
  Cheetah, … import them directly. Laps (manual and by distance), auto
  pause, ride summary and a ride history on the device.
* **Data pages**: up to 5 pages, 18 cell layouts, a field per cell — speed,
  distance, time, HR, cadence, power, altitude, gears, calories,
  zones, sunrise / sunset and more, each as current / min / max / avg.
* **Maps**: renders the Mapsforge `.map` files already on the device (or
  any official Mapsforge map), with the ridden track drawn on top.
* **Routes**: load a GPX track (or a past ride), preview its climb, ride it
  forwards or reversed, see the distance left.
* **Sensors**: ANT+ heart rate, speed, cadence, power, electronic shifting
  (SRAM AXS / eTap, Magene QED, Shimano Di2).
* **Rider profile**: calories (power → HR → speed, like the vendor), HR and
  power zones, time in zones.
* **Touch and keys**: swipe between pages, tap the gear icon for the
  settings; everything also works with the buttons.
* Dark / light theme (or automatic by sunrise / sunset), USB storage mode
  for copying maps, routes and rides.

## Supported devices

| Device | Build | Status |
|---|---|---|
| Magene C606 | `c606` (default) | verified on hardware |
| Magene C706 | `c706` | verified on hardware |
| Geoid CC700 Pro | `cc700pro` | verified on hardware (ANT+ sensors not yet checked) |
| Magene C606 Pro | `c606pro` | builds, not yet tried on a unit |

The images are not interchangeable — use the one for your device.

## Getting started

1. Get a release zip for your device (or [build one](docs/DEVELOPMENT.md)).
2. Follow [docs/FLASHING.md](docs/FLASHING.md): `pip install esptool`, run
   `flash_poc.py`. It backs up the vendor firmware first.
3. Read the [user guide](docs/USER_GUIDE.md) for the keys, pages and
   settings.

To go back: `flash_poc.py --restore backup-<date>-ota_0.bin`.

If the device ever hangs, **hold all three buttons** for a few seconds —
the coprocessor hard-resets the ESP32 whatever the firmware is doing.

## Documentation

* [Flashing](docs/FLASHING.md) — install, back up, restore
* [User guide](docs/USER_GUIDE.md) — keys, pages, fields, settings, maps, routes
* [Development](docs/DEVELOPMENT.md) — building, targets, developer console, code layout
* [Hardware notes](docs/HARDWARE.md) — reverse-engineered pin maps, protocols and vendor algorithms
* [Screenshots](docs/SCREENSHOTS.md)

## Under the hood

An ESP32-S3 runs the UI (LVGL 9) and talks to an nRF coprocessor that owns
the buttons, power and sensor radios; GPS is an Airoha receiver, storage an
on-board eMMC. Everything here was worked out from the vendor firmware —
see [HARDWARE.md](docs/HARDWARE.md).

## License

GPL-3.0-or-later, see [LICENSE](LICENSE).
