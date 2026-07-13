# Project Progress

This is the source of truth for current work across the office and apartment.
Update it in the same commit as the related code or test evidence.

## Current status

- Phase: P0 - Baseline and safety
- Overall state: planning complete, implementation not started
- Last updated: 2026-07-13
- Current firmware: v1.1.0
- Hardware location: apartment
- Next hardware session: not scheduled

## Immediate backlog

| ID | Task | Location | Status | Evidence |
|---|---|---|---|---|
| P0-01 | Remove firmware Wi-Fi credentials and add local secret handling | work | done | Offline build passed; current-tree secret scan clean; local header ignored |
| P0-02 | Capture a clean firmware build baseline | work | done | WSL Docker build: RAM 15.5%, Flash 27.9%, cached build 5.69s |
| P0-03 | Make motor stop interrupt active movement | work + apartment | needs-hardware | Cancellation generation and per-step position compiled; physical stop test pending |
| P0-04 | Release stepper coils after move/stop/error | work + apartment | needs-hardware | Release paths compiled; holding-current measurement pending |
| P0-05 | Add initial pan/tilt software limits | work + apartment | todo | |
| P0-06 | Correct timer deep-sleep wake setup | work + apartment | todo | |
| P0-07 | Measure baseline current for every hardware state | apartment | todo | |
| P0-08 | Add API mocks and host-side tests | work | todo | |
| P0-09 | Define versioned configuration schema | work | todo | |

Allowed status values: `todo`, `in-progress`, `needs-hardware`, `blocked`, `done`.

## Next actions

1. At work: implement motor cancellation and coil release behind tests/mocks.
2. At work: define the versioned configuration schema for later AP provisioning.
3. Before apartment visit: create `include/secrets.h` locally and rotate the old
   apartment Wi-Fi password because it remains in Git history.
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

## Build baseline

| Date | Environment | Result | RAM | Flash | Artifact |
|---|---|---|---:|---:|---:|
| 2026-07-13 | WSL2 Docker, PlatformIO 6.1.18, espressif32 7.0.1 | success | 50,944 / 327,680 bytes | 876,601 / 3,145,728 bytes | firmware.bin 877,024 bytes |

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
