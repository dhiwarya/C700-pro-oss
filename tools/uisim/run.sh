#!/bin/sh
# Builds the desktop renderer and renders every screen to PNG.
#   tools/uisim/run.sh [light|dark] [OUTDIR]
# Needs Python 3 with Pillow for the PNG conversion (PYTHON=... to choose one).
set -e
cd "$(dirname "$0")"
THEME=${1:-light}
OUT=${2:-out/$THEME}
PYTHON=${PYTHON:-python3}
make -s -j10
rm -rf "$OUT"
mkdir -p "$OUT"
./build/uisim "$OUT" "$THEME" > /dev/null
"$PYTHON" - "$OUT" <<'EOF'
import sys, glob, os
from PIL import Image
for p in sorted(glob.glob(os.path.join(sys.argv[1], "*.ppm"))):
    Image.open(p).save(p[:-4] + ".png")
    os.remove(p)
    print(p[:-4] + ".png")
EOF
