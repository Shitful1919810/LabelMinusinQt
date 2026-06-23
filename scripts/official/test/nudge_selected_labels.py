#!/usr/bin/env python3
import argparse
import json


def clamp_unit(value: float) -> float:
    return max(0.0, min(1.0, value))


def write_output(path: str, title: str, text: str, operations: list[dict]) -> None:
    with open(path, "w", encoding="utf-8") as output_file:
        json.dump(
            {
                "apiVersion": 1,
                "summary": text,
                "result": {
                    "type": "message",
                    "title": title,
                    "text": text,
                },
                "operations": operations,
            },
            output_file,
            ensure_ascii=False,
            indent=2,
        )


def parse_float(value: object, fallback: float) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return fallback


def main() -> None:
    parser = argparse.ArgumentParser(description="Nudge selected label marker positions.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as input_file:
        payload = json.load(input_file)

    parameters = payload.get("parameters", {})
    dx = parse_float(parameters.get("dx"), 0.02)
    dy = parse_float(parameters.get("dy"), 0.02)
    current_page = payload.get("context", {}).get("currentPage", {})
    page_name = current_page.get("name", "")
    selected_labels = payload.get("context", {}).get("selectedLabels", [])

    operations = []
    if page_name:
        for label in selected_labels:
            operations.append(
                {
                    "type": "setLabelPosition",
                    "page": page_name,
                    "labelIndex": label.get("labelIndex", -1),
                    "x": clamp_unit(float(label.get("x", 0.0)) + dx),
                    "y": clamp_unit(float(label.get("y", 0.0)) + dy),
                }
            )

    if operations:
        result_text = f"Nudged {len(operations)} selected label marker(s) by dx={dx}, dy={dy}."
    else:
        result_text = "Select one or more labels before running this script."
    write_output(args.output, "Set Label Position Test", result_text, operations)


if __name__ == "__main__":
    main()
