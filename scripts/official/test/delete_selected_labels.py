#!/usr/bin/env python3
import argparse
import json


def main() -> None:
    parser = argparse.ArgumentParser(description="Delete selected labels.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as input_file:
        payload = json.load(input_file)

    current_page = payload.get("context", {}).get("currentPage", {})
    page_name = current_page.get("name", "")
    selected_labels = payload.get("context", {}).get("selectedLabels", [])

    operations = []
    if page_name:
        for label in selected_labels:
            operations.append(
                {
                    "type": "deleteLabel",
                    "page": page_name,
                    "labelIndex": label.get("labelIndex", -1),
                }
            )

    if operations:
        result_text = f"Deleted {len(operations)} selected label(s). Use Undo to restore them."
    else:
        result_text = "Select one or more labels before running this script."

    with open(args.output, "w", encoding="utf-8") as output_file:
        json.dump(
            {
                "apiVersion": 1,
                "summary": result_text,
                "result": {
                    "type": "message",
                    "title": "Delete Label Test",
                    "text": result_text,
                },
                "operations": operations,
            },
            output_file,
            ensure_ascii=False,
            indent=2,
        )


if __name__ == "__main__":
    main()
