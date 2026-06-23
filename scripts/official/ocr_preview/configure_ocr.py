#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
from typing import Any

CONFIG_PATH = Path(__file__).with_name("config.json")


def read_input(path: str) -> dict[str, Any]:
    with open(path, "r", encoding="utf-8") as input_file:
        payload = json.load(input_file)
    return payload if isinstance(payload, dict) else {}


def read_existing_config() -> dict[str, Any]:
    if not CONFIG_PATH.exists():
        return {}
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as config_file:
            config = json.load(config_file)
        return config if isinstance(config, dict) else {}
    except Exception:
        return {}


def write_output(path: str, text: str) -> None:
    with open(path, "w", encoding="utf-8") as output_file:
        json.dump(
            {
                "apiVersion": 1,
                "summary": "OCR configuration saved.",
                "result": {
                    "type": "message",
                    "title": "OCR Configuration",
                    "text": text,
                },
            },
            output_file,
            ensure_ascii=False,
            indent=2,
        )


def as_bool(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "on"}
    return False


def main() -> None:
    parser = argparse.ArgumentParser(description="Configure LabelMinus OCR automation scripts.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    payload = read_input(args.input)
    parameters = payload.get("parameters", {})
    if not isinstance(parameters, dict):
        parameters = {}

    config = read_existing_config()
    config.update(
        {
            "engine": str(parameters.get("engine", config.get("engine", "paddle"))),
            "language": str(parameters.get("language", config.get("language", "japan"))),
            "device": str(parameters.get("device", config.get("device", "cpu"))),
            "mangaModelPath": str(parameters.get("mangaModelPath", config.get("mangaModelPath", ""))),
            "defaultGroup": str(parameters.get("defaultGroup", config.get("defaultGroup", "框内"))),
            "rightToLeft": as_bool(parameters.get("rightToLeft", config.get("rightToLeft", False))),
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
    ]
    write_output(args.output, "\n".join(lines))


if __name__ == "__main__":
    main()
