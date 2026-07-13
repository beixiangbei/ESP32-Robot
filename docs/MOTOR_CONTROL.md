# Motor Control and Calibration

## Coordinate model

Motor positions are logical step counts. Positive and negative positions are
independent from the physical coil direction, which can later be reversed through
versioned configuration.

Current development defaults:

| Axis | Minimum | Center | Maximum | Reversed |
|---|---:|---:|---:|---|
| pan | -1024 | 0 | 1024 | false |
| tilt | -256 | 0 | 256 | false |

These values only reject obviously excessive commands. They are not verified
mechanical limits and must be replaced with measured values at the apartment.
Saved values use versioned ESP32 NVS storage and survive a reboot.

## Control panel

Open the ESP32 device root URL in a browser:

```text
http://<device-ip>/
```

The responsive calibration panel provides pan and tilt point movement, stop and
coil release, estimated position, min/center/max capture, numeric limit editing
and direction reversal. Configuration changes are persisted only when a capture
or Save command succeeds.

The current development page is available on the trusted LAN without
authentication. Do not expose the ESP32 HTTP port to the internet. Authentication
is required before the broader control panel is considered production-ready.

All movement commands are checked twice: once before entering the queue and again
immediately before execution. This prevents queued relative commands from crossing
a limit after earlier commands change the current position.

## APIs

### Relative movement

```http
POST /api/v1/motor/relative
Content-Type: application/json

{"motor":0,"steps":100,"speed":8,"accel":true}
```

### Absolute movement

```http
POST /api/v1/motor/absolute
Content-Type: application/json

{"motor":1,"target":-100,"speed":8}
```

Negative absolute positions, including `-1`, are valid. A missing target returns
HTTP 400. A target outside the current soft limits also returns HTTP 400.

### Calibrate current physical position as center

Manually place the enclosure at its intended center, then call:

```http
POST /api/v1/motor/calibrate
Content-Type: application/json

{"motor":"pan"}
```

Valid values are `pan`, `tilt` and `all`. Calibration cancels old queued commands,
waits for active movement to acknowledge cancellation, releases the coils and sets
the estimated position to the configured center. It does not physically discover
the center and cannot detect manual movement or missed steps.

### Status

`GET /api/v1/motor/status` returns estimated position, busy state, limits, center
and direction reversal for both axes.

### Read and update persistent configuration

```http
GET /api/v1/motor/config

POST /api/v1/motor/config
Content-Type: application/json

{"motor":0,"min":-420,"center":0,"max":460,"reversed":false}
```

Updates are rejected while the axis is moving, when the range is invalid, or when
the current estimated position would be outside the new range.

### Capture a boundary

```http
POST /api/v1/motor/limit/capture
Content-Type: application/json

{"motor":0,"limit":"min"}
```

Valid limit names are `min`, `center` and `max`. Center capture cancels movement,
releases the motor and resets the current estimated coordinate to the configured
center. Min and max capture save the current estimated coordinate.

## Safety boundary

Software limits reduce command risk but do not replace an origin sensor or limit
switch. Reboot, manual movement, mechanical slip and missed motor steps invalidate
the estimated position. Re-calibrate before using the full travel range whenever
the physical position is uncertain.

## Host tests

```powershell
.\scripts\host-test.ps1
```

The pure C++ policy tests cover boundary acceptance, out-of-range rejection,
relative target overflow, invalid axes, negative absolute positions and direction
reversal.

Run the control panel against a local mock device at work with:

```powershell
python tools\control_panel_preview.py
```

Then open `http://127.0.0.1:8766`.
