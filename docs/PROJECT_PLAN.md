# MOSS Robot Project Plan

Last updated: 2026-07-13

## 1. Project objective

Build a MOSS-style desktop robot for long-running use in a rented apartment. The
ESP32-S3 is the hardware controller. An HPT630 mini PC will run the agent gateway
24/7. ASR, TTS, vision and language models are cloud services selected through
replaceable provider adapters.

The system must remain useful when cloud AI is unavailable: local web control,
power management, motor safety, display, camera capture and basic rules continue
to work without the agent.

## 2. Development constraints

- Hardware is in the apartment; daytime development usually happens at work.
- Repository and planning state are synchronized through GitHub.
- Work that needs no hardware must have mocks or recorded samples.
- Hardware-dependent work is not complete until verified at the apartment.
- Credentials, Wi-Fi passwords, tokens and apartment network details must never
  be committed.

## 3. Target architecture

```text
Cloud ASR / TTS / LLM / vision
              |
HPT630: agent-gateway (24/7)
  conversation, provider adapters, memory, rules, observability
              |
     LAN HTTP/WebSocket or MQTT
              |
ESP32-S3: deterministic device controller
  power, motors, camera, OLED, LED, audio, local web UI
```

Responsibilities are intentionally separated:

- ESP32: real-time hardware control, safety, sleep, local settings and fallback UI.
- HPT630: cloud credentials, dialogue, ASR/TTS streaming, model tools and history.
- Cloud: expensive inference only; no cloud provider is embedded in the firmware.

## 4. Delivery phases

### P0 - Baseline and safety

- Remove hard-coded Wi-Fi credentials and rotate the existing password.
- Split hardware modules from HTTP handlers without changing public behavior.
- Fix motor stop so it interrupts an active movement.
- Release stepper coils after movement and on every failure path.
- Add motor command timeout, queue limits and software travel limits.
- Fix timer wake-up and document all wake sources.
- Add a host-side API mock and basic unit tests.

Exit criteria: existing functions still work, stop is effective, coils are off at
idle, secrets are absent, and the firmware builds reproducibly.

### P1 - Power management

- Add `ACTIVE`, `IDLE`, `LIGHT_SLEEP` and `DEEP_SLEEP` states.
- Turn camera, I2S, OLED, LEDs and motor holding current on only when needed.
- Enable Wi-Fi modem sleep and configurable online/power-saving behavior.
- Add idle timers, quiet hours and low-battery policy.
- Expose current mode, wake reason and per-module active time in the API.
- Measure current in boot, motor moving, motor idle, camera, Wi-Fi idle, light
  sleep and deep sleep states.

Exit criteria: a measurement table exists, scheduled wake is reliable for 20
cycles, and a 72-hour battery test has no unexplained reset or lockup.

### P2 - Configuration and local control panel

- Add AP captive-portal provisioning with STA fallback.
- Store versioned settings in NVS and larger assets/rules in LittleFS.
- Add authentication, session expiry and protected dangerous operations.
- Build responsive pages for status, motor, camera, display, LED, audio, power,
  network, rules, logs, update and factory reset.
- Add WebSocket status updates and an emergency motor-stop control.
- Support settings export/import with secrets excluded by default.

Exit criteria: a phone can provision and operate a factory-reset device without
editing source code, and invalid configuration safely falls back to defaults.

### P3 - Motor calibration and rules

- Add manual center calibration and configurable pan/tilt min/max/direction.
- Route every movement source through one limit-checking controller.
- Mark position as estimated and provide a re-calibration workflow.
- Add structured triggers, conditions and actions; do not execute user scripts.
- Support time, battery, charging, idle, network and agent events.

Exit criteria: API, UI and rules cannot bypass limits. Position recovery behavior
after reboot and manual movement is documented and visible to the user.

### P4 - HPT630 agent gateway

- Run services with Docker Compose and automatic restart.
- Implement provider-neutral ASR, TTS, LLM and vision interfaces.
- Add device discovery, health checks, tool schemas and command auditing.
- Keep cloud API keys only on the HPT630.
- Add conversation cancellation, timeouts, rate limits and offline fallback.
- Add logs, latency metrics, token/cost accounting and provider health.

Exit criteria: changing a provider requires configuration plus one adapter, not
firmware changes; a failed cloud call cannot leave a motor or audio task running.

### P5 - Voice and embodied behavior

- Start with push-to-talk from the web UI to validate the full audio pipeline.
- Add local voice activity detection, interruption and streaming cloud ASR.
- Add cloud TTS streaming with playback cancellation.
- Evaluate an offline wake-word engine separately from configurable display text.
- Add MOSS expressions, attention direction and safe compound actions.

Exit criteria: wake, listen, respond, interrupt and timeout paths are tested, and
all physical actions remain subject to firmware safety limits.

### P6 - Operations

- Signed OTA with rollback, staged release channels and recovery instructions.
- Configuration schema migration and backup.
- Long-run, low-battery, network-loss and power-loss tests.
- Threat model for LAN access, cloud callbacks and update delivery.

## 5. Two-location workflow

### At work

- Pull before starting; update `docs/PROGRESS.md` before ending.
- Develop protocol, web UI, agent gateway, mocks, tests and documentation.
- Tag hardware-required items as `needs-hardware`; do not mark them done.
- Commit small changes with build/test evidence in the progress log.

### Before returning to the apartment

- Prepare a numbered test session in `docs/HARDWARE_TEST_LOG.md`.
- Include firmware revision, wiring assumptions, commands, expected results and
  rollback steps.
- Build firmware and prepare a short test sequence; avoid debugging from memory.

### At the apartment

- Pull, confirm the commit hash, photograph wiring changes and run the prepared
  test session.
- Record current, voltage, duration, serial logs, observed behavior and failures.
- Do not combine wiring changes, firmware changes and battery tests without
  recording each boundary.
- Push the results before leaving so work can continue from the same evidence.

## 6. Milestones

| Milestone | Result | Hardware required |
|---|---|---|
| M1 | Safe baseline and reproducible build | Yes |
| M2 | Measured multi-level power management | Yes |
| M3 | Provisioning and complete local control UI | Final verification |
| M4 | Calibration, limits and local rules | Yes |
| M5 | HPT630 agent gateway with provider adapters | Final integration |
| M6 | Cloud voice conversation and wake-word evaluation | Yes |
| M7 | OTA, recovery and long-run validation | Yes |

## 7. Non-negotiable engineering rules

- No secrets in Git, firmware binaries, logs or exported settings.
- No motor command bypasses the central safety controller.
- Software limits reduce risk but do not provide physical absolute positioning.
- Every power optimization is accepted from measurements, not assumptions.
- Cloud provider failures degrade features; they must not disable local control.
- Breaking API and configuration changes require versioning and migration notes.

