#!/usr/bin/env python3
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
errors = []

cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
for match in re.findall(r"(?:Source|Tests|assets)/[^\s\)]+", cmake):
    candidate = root / match.strip('"')
    if not candidate.exists():
        errors.append(f"Missing CMake input: {candidate.relative_to(root)}")

for svg in sorted((root / "assets/svg").glob("*.svg")):
    try:
        ET.parse(svg)
    except Exception as exc:
        errors.append(f"Invalid SVG {svg.name}: {exc}")

parameter_text = (root / "Source/Parameters.h").read_text(encoding="utf-8")
required = ["interval", "harmony", "formant", "humanize", "width", "mix", "key", "scale", "autoKey", "enabled"]
for parameter in required:
    if f'"{parameter}"' not in parameter_text:
        errors.append(f"Missing stable parameter ID: {parameter}")

mockup = root / "assets/mockup/NF_Vocal_Harmonizer_Approved.png"
if not mockup.exists() or mockup.stat().st_size < 100_000:
    errors.append("Approved mockup is missing or unexpectedly small")

if errors:
    print("PACKAGE VALIDATION FAILED")
    print("\n".join(f"- {e}" for e in errors))
    sys.exit(1)
print(f"PACKAGE VALIDATION PASS - {sum(1 for p in root.rglob('*') if p.is_file())} files")

