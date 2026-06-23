#!/usr/bin/env python3
import argparse
import json
import time


def main() -> None:
    parser = argparse.ArgumentParser(description="Count characters in all LabelMinus labels.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()
    
    time.sleep(5)
    with open(args.output, "w", encoding="utf-8") as output_file:
        json.dump({
            "apiVersion": 1,
            "summary": "Waited 5 seconds.",
            "result": {
                "type": "message",
                "title": "Wait 5s",
                "text": "Waited 5 seconds."
            },
            "quiet" : True
        }, output_file, ensure_ascii=False, indent=2)


if __name__ == "__main__":
    main()
