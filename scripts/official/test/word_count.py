#!/usr/bin/env python3
import argparse
import json


def main() -> None:
    parser = argparse.ArgumentParser(description="Count characters in all LabelMinus labels.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as input_file:
        payload = json.load(input_file)

    pages = payload.get("project", {}).get("pages", [])
    label_count = 0
    character_count = 0
    page_count = len(pages)
    group_counts = {}

    for page in pages:
        for label in page.get("labels", []):
            group = label.get("group", "")
            text = label.get("text", "")
            label_count += 1
            character_count += len(text)
            group_count = group_counts.setdefault(group, {"labels": 0, "characters": 0})
            group_count["labels"] += 1
            group_count["characters"] += len(text)

    summary = f"Counted {character_count} characters in {label_count} labels across {page_count} pages."
    group_lines = []
    for group in sorted(group_counts):
        display_group = group if group else "(empty group)"
        counts = group_counts[group]
        group_lines.append(
            f"{display_group}: {counts['characters']} characters in {counts['labels']} labels"
        )
    result_text = summary
    if group_lines:
        result_text = summary + "\n\nBy group:\n" + "\n".join(group_lines)

    with open(args.output, "w", encoding="utf-8") as output_file:
        json.dump(
            {
                "apiVersion": 1,
                "summary": summary,
                "result": {
                    "type": "message",
                    "title": "Label Word Count",
                    "text": result_text,
                },
            },
            output_file,
            ensure_ascii=False,
            indent=2,
        )


if __name__ == "__main__":
    main()
