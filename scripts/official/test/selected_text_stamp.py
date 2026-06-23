#!/usr/bin/env python3
import argparse
import json


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


def main() -> None:
    parser = argparse.ArgumentParser(description="Append a text stamp to selected labels.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as input_file:
        payload = json.load(input_file)

    parameters = payload.get("parameters", {})
    stamp = str(parameters.get("stamp", " [automation]"))
    current_page = payload.get("context", {}).get("currentPage", {})
    page_name = current_page.get("name", "")
    selected_labels = payload.get("context", {}).get("selectedLabels", [])

    operations = []
    if page_name:
        for label in selected_labels:
            operations.append(
                {
                    "type": "setLabelText",
                    "page": page_name,
                    "labelIndex": label.get("labelIndex", -1),
                    "text": str(label.get("text", "")) + stamp,
                }
            )

    if operations:
        result_text = f"Stamped {len(operations)} selected label(s)."
    else:
        result_text = "Select one or more labels before running this script."
    write_output(args.output, "Set Label Text Test", result_text, operations)


if __name__ == "__main__":
    main()
