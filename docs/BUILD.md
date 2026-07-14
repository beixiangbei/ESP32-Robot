# Firmware Build

The supported reproducible build runs PlatformIO Core 6.1.18 in the Docker Engine
installed inside WSL. Docker Desktop and Docker Compose are not required.

## Windows entry point

From the repository root:

```powershell
.\scripts\firmware-build.ps1
```

The first run downloads the Python base image, PlatformIO packages, the ESP32
toolchain and Arduino libraries. Later builds reuse the named Docker volume
`moss-platformio-cache`.

## WSL entry point

```sh
./scripts/firmware-build.sh
```

Additional arguments are passed to `pio run`. For example, a verbose build is:

```powershell
.\scripts\firmware-build.ps1 -v
```

Run host-side tests for hardware-independent policy code with:

```powershell
.\scripts\host-test.ps1
```

## Outputs

PlatformIO writes build output under:

```text
.pio/build/esp32-s3-devkitc-1/
```

The `.pio` directory is intentionally excluded from Git. Firmware must be built
from a recorded Git commit before hardware verification.

## Wi-Fi provisioning

Wi-Fi credentials are configured from the embedded control panel and stored in
ESP32 NVS. Source builds do not contain apartment credentials.

An unconfigured device or a device that cannot connect within 20 seconds starts
`MOSS-Setup-XXXX` with password `moss-setup`. Connect to it and open
`http://192.168.4.1/`.

The password previously stored in `src/main.cpp` remains in Git history and must
be rotated before the device is used again.

## Environment overrides

- `MOSS_FIRMWARE_IMAGE`: local image name/tag.
- `MOSS_PLATFORMIO_CACHE`: named volume used for PlatformIO downloads.

## Troubleshooting

- `docker compose` is not used because the current WSL Docker installation does
  not include the Compose plugin.
- A first-run failure while pulling images or packages usually indicates company
  network, DNS, proxy or registry restrictions. Preserve the exact error in
  `docs/PROGRESS.md`.
- Upload and serial monitoring are intentionally outside the company build flow;
  perform them where the ESP32 hardware is physically available.
