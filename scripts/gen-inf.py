#!/usr/bin/env python3
"""Generate Windows INF templates from metadata."""

from __future__ import annotations

import json
import pathlib
from string import Template

ROOT = pathlib.Path(__file__).resolve().parents[1]
META_PATH = ROOT / "packaging" / "windows" / "driver.meta.json"
TEMPLATE_DIR = ROOT / "packaging" / "windows" / "templates"
DIST_DIR = ROOT / "dist" / "windows"

TARGETS = {
    "win9x": {
        "template": "win9x.inf.in",
        "output": "win9x/lltd.inf",
    },
    "win16": {
        "template": "win16-oemsetup.inf.in",
        "output": "win16/oemsetup.inf",
    },
    "nt3x": {
        "template": "nt3x-oemsetup.inf.in",
        "output": "nt3x/oemsetup.inf",
    },
    "nt4": {
        "template": "nt4-oemsetup.inf.in",
        "output": "nt4/oemsetup.inf",
    },
    "w2k": {
        "template": "w2k.inf.in",
        "output": "w2k/lltd.inf",
    },
}


def render_template(template_path: pathlib.Path, data: dict[str, str]) -> str:
    raw = template_path.read_text(encoding="utf-8")
    return Template(raw).substitute(data)


def main() -> None:
    meta = json.loads(META_PATH.read_text(encoding="utf-8"))
    base = {
        "driver_name": meta["driver_name"],
        "display_name": meta["display_name"],
        "provider": meta["provider"],
        "version": meta["version"],
    }
    targets = meta.get("targets", {})
    for target_name, target_meta in TARGETS.items():
        template = TEMPLATE_DIR / target_meta["template"]
        output = DIST_DIR / target_meta["output"]
        target_info = targets.get(target_name, {})
        data = {
            **base,
            "device_id": target_info.get("device_id", "LLTDRES"),
            "service_name": target_info.get("service_name", "LLTD"),
            "os_tag": target_info.get("os_tag", target_name),
        }
        output.parent.mkdir(parents=True, exist_ok=True)
        rendered = render_template(template, data)
        output.write_text(rendered, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
