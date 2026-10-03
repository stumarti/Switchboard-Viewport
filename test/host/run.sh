#!/usr/bin/env bash
# Host tests: the firmware's pure logic and its drawing, built with g++ and
# run on this machine — no ESP32, no PlatformIO.
#
#   test/host/run.sh
#
# Renders every fixture screen to test/host/out/*.png (PNG needs Python's
# Pillow; otherwise the .ppm files are left).
set -euo pipefail
cd "$(dirname "$0")/../.."
BUILD=test/host/build
OUT=test/host/out
mkdir -p "$BUILD" "$OUT"

# ArduinoJson is a single header; fetch the pinned release once.
AJ_VERSION=7.4.2
AJ="$BUILD/ArduinoJson.h"
if [ ! -f "$AJ" ]; then
  curl -sSfL -o "$AJ" "https://github.com/bblanchon/ArduinoJson/releases/download/v${AJ_VERSION}/ArduinoJson-v${AJ_VERSION}.h"
fi

CXX=${CXX:-g++}
FLAGS=(-std=gnu++17 -O1 -g -DSB_HOST -DARDUINOJSON_DEFAULT_NESTING_LIMIT=32 -Wall -Wno-unused-function -Wno-sign-compare -Itest/host/stubs -Itest/host -Isrc -I"$BUILD")
RENDER_SRC=(src/render/draw.cpp src/render/icons.cpp src/render/screens.cpp test/host/stubs/Adafruit_GFX.cpp)

echo "== logic tests"
"$CXX" "${FLAGS[@]}" -o "$BUILD/tests" test/host/test_main.cpp src/app/carousel.cpp
"$BUILD/tests"

echo "== render"
"$CXX" "${FLAGS[@]}" -o "$BUILD/render" test/host/render.cpp "${RENDER_SRC[@]}"
"$BUILD/render" "$OUT" test/fixtures/*.json
"$CXX" "${FLAGS[@]}" -o "$BUILD/system" test/host/system.cpp src/render/system.cpp lib/qrcodegen/qrcodegen.c -Ilib/qrcodegen "${RENDER_SRC[@]}"
"$BUILD/system" "$OUT"

if python3 -c "import PIL" 2>/dev/null; then
  python3 - "$OUT" <<'PY'
import glob, os, sys
from PIL import Image
for p in glob.glob(os.path.join(sys.argv[1], '*.ppm')):
    Image.open(p).save(p[:-4] + '.png')
    os.remove(p)
PY
fi
echo "Rendered into $OUT"
