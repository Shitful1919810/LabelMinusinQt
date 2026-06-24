#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "sdk"))
from labelqt_automation import AutomationContext, as_bool

CONFIG_PATH = Path(__file__).with_name("config.json")


def read_existing_config() -> dict[str, Any]:
    if not CONFIG_PATH.exists():
        return {}
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as config_file:
            config = json.load(config_file)
        return config if isinstance(config, dict) else {}
    except Exception:
        return {}


def main() -> None:
    parser = argparse.ArgumentParser(description="Configure LabelQt OCR automation scripts.")
    parser.add_argument("--input", required=True, help="Path to the LabelQt automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    ctx = AutomationContext.from_file(args.input)
    parameters = ctx.parameters

    config = read_existing_config()
    config.update(
        {
            "engine": str(parameters.get("engine", config.get("engine", "paddle"))),
            "language": str(parameters.get("language", config.get("language", "japan"))),
            "device": str(parameters.get("device", config.get("device", "cpu"))),
            "mangaModelPath": str(parameters.get("mangaModelPath", config.get("mangaModelPath", ""))),
            "defaultGroup": str(parameters.get("defaultGroup", config.get("defaultGroup", "框内"))),
            "rightToLeft": as_bool(parameters.get("rightToLeft", config.get("rightToLeft", True))),
            "showResult": as_bool(parameters.get("showResult", config.get("showResult", True))),
        }
    )

    with open(CONFIG_PATH, "w", encoding="utf-8") as config_file:
        json.dump(config, config_file, ensure_ascii=False, indent=2)
        config_file.write("\n")

    lines = [
        f"Saved OCR configuration to {CONFIG_PATH}",
        "",
        f"engine: {config['engine']}",
        f"language: {config['language']}",
        f"device: {config['device']}",
        f"mangaModelPath: {config['mangaModelPath'] or '(auto/local HuggingFace cache)'}",
        f"defaultGroup: {config['defaultGroup']}",
        f"rightToLeft: {config['rightToLeft']}",
        f"showResult: {config['showResult']}",
    ]
    ctx.write_output(args.output, "OCR Configuration", "\n".join(lines), summary="OCR configuration saved.")


if __name__ == "__main__":
    main()
