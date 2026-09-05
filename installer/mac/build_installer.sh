#!/usr/bin/env bash
set -euo pipefail

PLUGIN_NAME="NF Vocal Harmonizer"
VERSION="1.0.0"
ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
INSTALLER_DIR="$ROOT_DIR/installer/mac"
# Same layout as Vocal Verb: AU/VST3/AAX from one Release tree built with AAX SDK.
ART="${NF_ARTEFACTS:-$ROOT_DIR/build-macos/NFVocalHarmonizer_artefacts/Release}"
OUT_DIR="${NF_INSTALLER_OUT:-$ROOT_DIR/dist/installers}"

VST3="$ART/VST3/NF Vocal Harmonizer.vst3"
AU="$ART/AU/NF Vocal Harmonizer.component"
AAX="$ART/AAX/NF Vocal Harmonizer.aaxplugin"

for path in "$VST3" "$AU" "$AAX"; do
  if [[ ! -e "$path" ]]; then
    echo "error: missing $path" >&2
    echo "Build with AAX SDK first, e.g.:" >&2
    echo "  cmake -S . -B build-macos -DJUCE_DIR=\$HOME/Downloads/JUCE \\" >&2
    echo "    -DNF_ENABLE_AAX=ON -DJUCE_AAX_SDK_PATH=\$HOME/Downloads/aax-sdk-2-9-0 \\" >&2
    echo "    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64" >&2
    echo "  cmake --build build-macos --config Release --target \\" >&2
    echo "    NFVocalHarmonizer_AU NFVocalHarmonizer_VST3 NFVocalHarmonizer_AAX" >&2
    exit 1
  fi
done

# NFVocalHarmonizer alone only builds SharedCode — AU/VST3 must be explicit format targets.
AU_BIN="$AU/Contents/MacOS/NF Vocal Harmonizer"
VST3_BIN="$VST3/Contents/MacOS/NF Vocal Harmonizer"
check_marker() {
  local bin="$1"
  local needle="$2"
  python3 - "$bin" "$needle" <<'PY'
import sys
path, needle = sys.argv[1], sys.argv[2].encode()
data = open(path, "rb").read()
sys.exit(0 if needle in data else 1)
PY
}
for bin in "$AU_BIN" "$VST3_BIN"; do
  if [[ ! -f "$bin" ]]; then
    echo "error: missing plugin binary $bin" >&2
    echo "Rebuild format targets: NFVocalHarmonizer_AU NFVocalHarmonizer_VST3 NFVocalHarmonizer_AAX" >&2
    exit 1
  fi
  if ! check_marker "$bin" "DETECTING..."; then
    echo "error: $bin looks stale (missing DETECTING... marker)." >&2
    exit 1
  fi
  if ! check_marker "$bin" "CORRECTION:"; then
    echo "error: $bin looks stale (missing vocal-blob CORRECTION HUD)." >&2
    exit 1
  fi
  if ! check_marker "$bin" "FLAT"; then
    echo "error: $bin looks stale (missing FLAT editor tool)." >&2
    exit 1
  fi
done

AAX_BIN="$AAX/Contents/MacOS/NF Vocal Harmonizer"
if [[ ! -f "$AAX_BIN" ]]; then
  echo "error: AAX bundle has no binary (rebuild NFVocalHarmonizer_AAX with JUCE_AAX_SDK_PATH)" >&2
  exit 1
fi
if [[ $(stat -f%z "$AAX_BIN") -lt 100000 ]]; then
  echo "error: AAX binary looks truncated: $AAX_BIN" >&2
  exit 1
fi
echo "Using AAX from SDK build: $AAX ($(du -sh "$AAX" | awk '{print $1}'))"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/vst3/Library/Audio/Plug-Ins/VST3"
mkdir -p "$WORK/au/Library/Audio/Plug-Ins/Components"
mkdir -p "$WORK/aax/Library/Application Support/Avid/Audio/Plug-Ins"

cp -R "$VST3" "$WORK/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU" "$WORK/au/Library/Audio/Plug-Ins/Components/"
cp -R "$AAX" "$WORK/aax/Library/Application Support/Avid/Audio/Plug-Ins/"

SCRIPTS="$WORK/scripts"
mkdir -p "$SCRIPTS"
cat > "$SCRIPTS/postinstall" <<'EOF'
#!/bin/bash
set -euo pipefail
CONSOLE_USER="$(stat -f '%Su' /dev/console)"
USER_HOME="$(dscl . -read "/Users/${CONSOLE_USER}" NFSHomeDirectory 2>/dev/null | awk '{print $2}')"
if [[ -n "${USER_HOME:-}" && -d "$USER_HOME" ]]; then
    rm -rf "$USER_HOME/Library/Audio/Plug-Ins/VST3/NF Vocal Harmonizer.vst3"
    rm -rf "$USER_HOME/Library/Audio/Plug-Ins/Components/NF Vocal Harmonizer.component"
fi
# Refresh AU cache best-effort
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
EOF
chmod 755 "$SCRIPTS/postinstall"

PKG_DIR="$WORK/pkgs"
mkdir -p "$PKG_DIR"
pkgbuild --root "$WORK/vst3" --scripts "$SCRIPTS" \
  --identifier com.nfaudiotools.nfvocalharmonizer.vst3 --version "$VERSION" \
  --install-location / "$PKG_DIR/NFVocalHarmonizer-VST3.pkg"
pkgbuild --root "$WORK/au" \
  --identifier com.nfaudiotools.nfvocalharmonizer.au --version "$VERSION" \
  --install-location / "$PKG_DIR/NFVocalHarmonizer-AU.pkg"
pkgbuild --root "$WORK/aax" \
  --identifier com.nfaudiotools.nfvocalharmonizer.aax --version "$VERSION" \
  --install-location / "$PKG_DIR/NFVocalHarmonizer-AAX.pkg"

mkdir -p "$OUT_DIR"
productbuild \
  --distribution "$INSTALLER_DIR/Distribution.xml" \
  --resources "$INSTALLER_DIR" \
  --package-path "$PKG_DIR" \
  "$WORK/NF Vocal Harmonizer Installer.pkg"

DMG_STAGING="$WORK/dmg"
mkdir -p "$DMG_STAGING"
cp "$WORK/NF Vocal Harmonizer Installer.pkg" "$DMG_STAGING/"
mkdir -p "$DMG_STAGING/Manuals"
if [[ -f "$ROOT_DIR/Manuals/pdf/NF_Vocal_Harmonizer_User_Manual_English.pdf" ]]; then
  cp "$ROOT_DIR/Manuals/pdf/NF_Vocal_Harmonizer_User_Manual_English.pdf" "$DMG_STAGING/Manuals/"
  cp "$ROOT_DIR/Manuals/pdf/NF_Vocal_Harmonizer_Manual_Portugues.pdf" "$DMG_STAGING/Manuals/"
fi
cat > "$DMG_STAGING/Read Me.txt" <<'EOF'
NF Vocal Harmonizer v1.0
NF Audio Tools — By Nenno Fernando

1. Open "NF Vocal Harmonizer Installer.pkg"
2. Keep VST3, AU and AAX selected
3. Enter your Mac password when asked
4. Rescan plugins in your DAW

Install paths:
• VST3 → /Library/Audio/Plug-Ins/VST3
• AU   → /Library/Audio/Plug-Ins/Components
• AAX  → /Library/Application Support/Avid/Audio/Plug-Ins

User manuals (EN / PT) are embedded in the plugin.
Open them from the ≡ menu in the header.

Editor tools: SEL, FLAT (pencil), LINE (slope pencil), CUT (scissors).
ANALYZE can append later bars without overwriting earlier captures.
Default preset restores factory parameters.

© 2026 NF Audio Tools / Nenno Fernando
All rights reserved.
EOF

hdiutil create -volname "$PLUGIN_NAME" -srcfolder "$DMG_STAGING" -ov -format UDZO \
  "$OUT_DIR/NF Vocal Harmonizer - Mac Installer.dmg"

cp "$OUT_DIR/NF Vocal Harmonizer - Mac Installer.dmg" \
   "$HOME/Desktop/NF Vocal Harmonizer - Mac Installer.dmg"

echo "Created:"
echo "  $OUT_DIR/NF Vocal Harmonizer - Mac Installer.dmg"
echo "  $HOME/Desktop/NF Vocal Harmonizer - Mac Installer.dmg"
ls -lh "$OUT_DIR/NF Vocal Harmonizer - Mac Installer.dmg"
# Confirm AAX came from SDK-enabled artefacts
ls -lh "$AAX"
