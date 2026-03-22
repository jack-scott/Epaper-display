#!/usr/bin/env bash
set -e

PLATFORMIO=/home/jack/.platformio/penv/bin/platformio

$PLATFORMIO run --target upload "$@"
