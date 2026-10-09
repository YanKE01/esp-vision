# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Restore pinned experiment assets and verify their recorded SHA-256 hashes."""
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parent
for entry in json.loads((ROOT / "sources.json").read_text()):
    path = (ROOT / entry["path"]).resolve()
    if not path.is_relative_to(ROOT):
        raise ValueError("Asset path must remain inside the experiment directory")
    if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]:
        print("verified", entry["path"])
        continue
    data = urllib.request.urlopen(entry["url"], timeout=120).read()
    if hashlib.sha256(data).hexdigest() != entry["sha256"]:
        raise ValueError(f"Checksum mismatch: {entry['path']}")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".part")
    temporary.write_bytes(data)
    temporary.replace(path)
    print("downloaded", entry["path"])
