#!/usr/bin/env python3
import argparse
import json
import sys
import time


def main() -> None:
    parser = argparse.ArgumentParser(description="Emit stdout and stderr lines for automation log testing.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    for index in range(1, 6):
        print(f"stdout progress {index}/5", flush=True)
        print(f"stderr progress {index}/5", file=sys.stderr, flush=True)
        time.sleep(0.5)

    with open(args.output, "w", encoding="utf-8") as output_file:
        json.dump(
            {
                "apiVersion": 1,
                "summary": "Realtime log test finished.",
                "result": {
                    "type": "message",
                    "title": "Realtime Log Test",
                    "text": "Realtime log test finished.",
                },
            },
            output_file,
            ensure_ascii=False,
            indent=2,
        )


if __name__ == "__main__":
    main()
