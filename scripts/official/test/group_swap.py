#!/usr/bin/env python3
import argparse
import json


def main() -> None:
    parser = argparse.ArgumentParser(description="Swap all labels between two LabelMinus groups.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as input_file:
        payload = json.load(input_file)

    parameters = payload.get("parameters", {})
    group_a = parameters.get("groupA", "")
    group_b = parameters.get("groupB", "")
    operations = []

    if group_a and group_b and group_a != group_b:
        pages = payload.get("project", {}).get("pages", [])
        for page in pages:
            page_name = page.get("name", "")
            for label in page.get("labels", []):
                group = label.get("group", "")
                if group == group_a:
                    operations.append(
                        {
                            "type": "setLabelGroup",
                            "page": page_name,
                            "labelIndex": label.get("labelIndex", -1),
                            "group": group_b,
                        }
                    )
                elif group == group_b:
                    operations.append(
                        {
                            "type": "setLabelGroup",
                            "page": page_name,
                            "labelIndex": label.get("labelIndex", -1),
                            "group": group_a,
                        }
                    )

    count = len(operations)
    if not group_a or not group_b:
        result_text = "Please select two groups before running this script."
    elif group_a == group_b:
        result_text = "The two selected groups are the same, so no labels were changed."
    else:
        result_text = f"Swapped {count} labels between {group_a} and {group_b}."

    with open(args.output, "w", encoding="utf-8") as output_file:
        json.dump(
            {
                "apiVersion": 1,
                "summary": result_text,
                "result": {
                    "type": "message",
                    "title": "Swap Label Groups",
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
