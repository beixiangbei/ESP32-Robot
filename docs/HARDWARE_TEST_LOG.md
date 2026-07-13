# Hardware Test Log

Do not overwrite old sessions. Add a new numbered section for each visit.

## Device inventory

| Item | Model/revision | Notes |
|---|---|---|
| Controller | ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM |
| Battery | 10000 mAh, details pending | Record cell and converter details |
| Host | HPT630 mini PC, details pending | Planned 24/7 agent host |

## Power measurement matrix

| Firmware commit | State | Battery voltage | Current | Duration | Notes |
|---|---|---:|---:|---:|---|
| pending | Power off | | | | |
| pending | Boot | | | | |
| pending | Wi-Fi idle | | | | |
| pending | Motor moving | | | | |
| pending | Motor stopped before coil-release fix | | | | |
| pending | Motor stopped after coil-release fix | | | | |
| pending | Camera enabled | | | | |
| pending | Light sleep | | | | |
| pending | Deep sleep | | | | |

## T001 - Baseline power and motor hold current

- Status: planned
- Required commit: to be assigned
- Goal: identify idle consumers and quantify stepper holding-current impact.
- Equipment: multimeter or USB power meter, charged battery, serial console.

Procedure:

1. Record wiring, battery voltage, converter model and firmware commit.
2. Measure boot peak and five-minute Wi-Fi idle current.
3. Move each motor, then measure current for two minutes without another command.
4. Send motor stop and repeat the measurement.
5. Disable LEDs, OLED, camera and audio one at a time; record each change.
6. Run light/deep sleep tests only after wake configuration is reviewed.

Motor cancellation checks:

1. Send a long pan movement and issue `motor/stop` while it is visibly moving.
2. Record stop latency and confirm reported position does not jump to the old target.
3. Queue three pan movements, stop during the first, and confirm the other two old
   commands do not run.
4. Repeat for tilt, then verify stopping pan does not discard a queued tilt command.
5. After normal completion and after stop, read `/api/v1/debug`; `motorState` must
   show the stopped motor nibble as zero.
6. Compare battery current while moving and for two minutes after completion. The
   post-movement current must return to the non-motor idle baseline.

Soft-limit and calibration checks:

1. Manually position both axes at the intended center and call `motor/calibrate`.
2. Move toward each physical boundary in small increments and record the last safe
   logical position; do not use the development defaults as a physical target.
3. Verify commands one step inside each measured boundary are accepted and commands
   one step outside are rejected without movement.
4. Reboot and confirm the UI/API clearly treats the restored position as estimated.
5. Determine whether pan or tilt direction must be reversed in configuration.
6. Open the root control page from a real phone and verify both axis panels fit
   without horizontal scrolling at the browser's default zoom.
7. Save limits and direction, reboot the ESP32, and confirm the same values load
   from NVS.

Acceptance evidence:

- Raw measurements entered in the matrix.
- Serial log attached or summarized.
- Any reset, heat, noise, missed step or network loss recorded.
- P1 current targets updated from measured results.
