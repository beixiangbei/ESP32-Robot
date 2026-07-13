#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
IMAGE_NAME=${MOSS_FIRMWARE_IMAGE:-moss-firmware-build:pio-6.1.18}

docker build \
  --file "$REPO_DIR/Dockerfile.firmware" \
  --tag "$IMAGE_NAME" \
  "$REPO_DIR"

docker run --rm \
  --volume "$REPO_DIR:/workspace" \
  "$IMAGE_NAME" \
  sh -c 'mkdir -p /tmp/moss-tests && g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Ilib/motor_policy/src lib/motor_policy/src/motor_policy.cpp test/host_motor_policy.cpp -o /tmp/moss-tests/motor_policy && /tmp/moss-tests/motor_policy'

