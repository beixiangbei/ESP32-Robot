# Project Progress

This is the source of truth for current work across the office and apartment.
Update it in the same commit as the related code or test evidence.

## Current status

- Phase: P0 - Baseline and safety
- Overall state: baseline safety and unified control panel implemented; hardware verification pending
- Last updated: 2026-07-14
- Current firmware: v1.1.0
- Hardware location: apartment
- Next hardware session: not scheduled

## Immediate backlog

| ID | Task | Location | Status | Evidence |
|---|---|---|---|---|
| P0-01 | Remove firmware Wi-Fi credentials and add provisioning | work | done | Credentials removed; versioned NVS and AP provisioning compiled |
| P0-02 | Capture a clean firmware build baseline | work | done | WSL Docker build: RAM 15.5%, Flash 27.9%, cached build 5.69s |
| P0-03 | Make motor stop interrupt active movement | work + apartment | needs-hardware | Cancellation generation and per-step position compiled; physical stop test pending |
| P0-04 | Release stepper coils after move/stop/error | work + apartment | needs-hardware | Release paths compiled; holding-current measurement pending |
| P0-05 | Add initial pan/tilt software limits | work + apartment | needs-hardware | NVS limits, capture API and embedded calibration panel implemented; phone/hardware ranges pending |
| P0-06 | Correct timer deep-sleep wake setup | work + apartment | todo | |
| P0-07 | Measure baseline current for every hardware state | apartment | todo | |
| P0-08 | Add API mocks and host-side tests | work | done | Motor policy tests pass; unified panel mock covers every API currently used by the UI |
| P0-09 | Define versioned configuration schema | work | in-progress | Versioned motor NVS config implemented; remaining modules pending |
| P0-10 | AP provisioning and Wi-Fi recovery | work + apartment | needs-hardware | Scan/config/forget APIs, captive DNS, OLED setup info and AP fallback compiled |

## Unified control panel

- Single source: `web/control-panel.html`, embedded into firmware by `scripts/embed_control_panel.py`
- Preview command: `python tools/control_panel_preview.py`
- Preview URL: `http://127.0.0.1:8766`
- Views: overview, motion, camera, expression, voice/agent, automation, settings.
- Visual direction: Chinese Apple-style light system UI with responsive navigation.
- System, motor, network, camera, OLED, LED, audio and system-control views use
  matching firmware and mock APIs.
- Agent, automation and power-policy fields are capability-gated until their
  backends exist; the panel does not report fake saves.
- Desktop, overview mobile and settings mobile screenshots checked without overlap.

Allowed status values: `todo`, `in-progress`, `needs-hardware`, `blocked`, `done`.

## Next actions

1. At work: fix deep-sleep timer wake and extract its time calculation for tests.
2. At work: define the versioned configuration schema for later AP provisioning.
3. At apartment: connect to `MOSS-Setup-XXXX`, provision the new Wi-Fi password,
   and confirm NVS reconnect and AP fallback behavior.
4. Before apartment visit: prepare hardware session T001.
5. At apartment: measure baseline current before and after coil release.

## Open questions

- Exact HPT630 CPU, RAM, storage, OS and whether Docker is already installed.
- Battery cell arrangement, protection board, charger and voltage-converter model.
- Measured current at the battery, not only the USB input.
- Whether a physical wake/config button is available on the enclosure.
- Whether the apartment network allows stable LAN addressing or mDNS.
- Preferred cloud region, monthly budget and tolerance for overseas API latency.
- PlatformIO currently reports an 8 MB/no-PSRAM board while the documented module
  is N16R8; verify the module marking before changing board memory settings.

## Decision log

| Date | Decision | Reason |
|---|---|---|
| 2026-07-13 | ESP32 remains the safety and local-control authority | Physical safety must not depend on cloud availability |
| 2026-07-13 | HPT630 hosts the 24/7 agent gateway | Keeps credentials and changing AI integrations off the MCU |
| 2026-07-13 | ASR, TTS, LLM and vision use provider adapters | Provider selection is still open and should remain replaceable |
| 2026-07-13 | Hardware verification is tracked separately from implementation | Hardware is only available at the apartment |
| 2026-07-13 | Firmware builds use pinned PlatformIO dependencies in WSL Docker | Windows has no Docker runtime and both locations need reproducible builds |
| 2026-07-13 | Deep Sleep is disabled until a reliable wake path exists | Normal operation must remain remotely reachable; HPT630 USB wake will be evaluated later |

## Build baseline

| Date | Environment | Result | RAM | Flash | Artifact |
|---|---|---|---:|---:|---:|
| 2026-07-13 | WSL2 Docker, PlatformIO 6.1.18, espressif32 7.0.1 | success | 50,944 / 327,680 bytes | 876,601 / 3,145,728 bytes | firmware.bin 877,024 bytes |
| 2026-07-14 | WSL2 Docker, unified control panel and provisioning | success | 53,636 / 327,680 bytes | 967,861 / 3,145,728 bytes | firmware.bin 968,272 bytes |

Notes:

- First build populated the `moss-platformio-cache` Docker volume.
- Cached incremental build completed in 5.69 seconds.
- Docker emitted only a legacy-builder deprecation notice; it did not affect the
  firmware build.

## Session handoff template

```text
Date/location:
Branch and commit:
Completed:
Tests/builds:
Hardware verification:
Known failures:
Next exact action:
Uncommitted local state:
```
