#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
IMAGE_NAME=${MOSS_FIRMWARE_IMAGE:-moss-firmware-build:pio-6.1.18}
CACHE_VOLUME=${MOSS_PLATFORMIO_CACHE:-moss-platformio-cache}

echo "[firmware-build] repository: $REPO_DIR"
echo "[firmware-build] image: $IMAGE_NAME"

docker build \
  --file "$REPO_DIR/Dockerfile.firmware" \
  --tag "$IMAGE_NAME" \
  "$REPO_DIR"

docker volume create "$CACHE_VOLUME" >/dev/null

docker run --rm \
  --volume "$REPO_DIR:/workspace" \
  --volume "$CACHE_VOLUME:/root/.platformio" \
  "$IMAGE_NAME" \
  pio run "$@"

