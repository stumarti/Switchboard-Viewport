#!/usr/bin/env bash
# The kitchen panel's own firmware vs Switchboard Server + this firmware, on
# the same Home Assistant, pixel for pixel (see harness.cpp). Exits non-zero
# if any pixel differs; images in test/compare/out/.
#
# To re-capture: run the server's demo (node tools/demo/demo.js in
# Switchboard-Server), then `node test/compare/capture.js` and
# `SERVER_DIR=../Switchboard-Server node test/compare/server-state.js`.
set -euo pipefail
cd "$(dirname "$0")/../.."
BUILD=test/host/build
OUT=test/compare/out
mkdir -p "$BUILD" "$OUT"
AJ_VERSION=7.4.2
[ -f "$BUILD/ArduinoJson.h" ] || curl -sSfL -o "$BUILD/ArduinoJson.h" "https://github.com/bblanchon/ArduinoJson/releases/download/v${AJ_VERSION}/ArduinoJson-v${AJ_VERSION}.h"
${CXX:-g++} -std=gnu++17 -O1 -DSB_HOST -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -w \
  -include test/compare/stubs/kd_env.h \
  -Isrc -Itest/compare/stubs -Itest/host -Itest/host/stubs -Isrc/kd -Itest/compare/kitchen-dash -I"$BUILD" \
  -o "$BUILD/kd-compare" \
  test/compare/harness.cpp test/compare/kitchen-dash/main.cpp test/compare/kitchen-dash/screen_status.cpp \
  test/compare/kitchen-dash/screen_heating.cpp test/compare/kitchen-dash/screen_security.cpp \
  test/compare/kitchen-dash/screen_charge.cpp test/compare/kitchen-dash/screen_error.cpp test/compare/kitchen-dash/screen_help.cpp \
  src/render/system.cpp lib/qrcodegen/qrcodegen.c -Ilib/qrcodegen \
  src/render/draw.cpp src/render/icons.cpp src/render/screens.cpp test/host/stubs/Adafruit_GFX.cpp
set +e
"$BUILD/kd-compare" "$OUT"
status=$?
set -e
if python3 -c "import PIL" 2>/dev/null; then
  python3 - "$OUT" <<'PY'
import glob, os, sys
from PIL import Image
for p in glob.glob(os.path.join(sys.argv[1], '*.ppm')):
    Image.open(p).save(p[:-4] + '.png'); os.remove(p)
PY
fi
exit $status
