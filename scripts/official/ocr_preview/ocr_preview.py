#!/usr/bin/env python3
import argparse
import json
import os
import tempfile
from pathlib import Path
from typing import Any

os.environ.setdefault("FLAGS_allocator_strategy", "auto_growth")
os.environ.setdefault("FLAGS_use_mkldnn", "0")
os.environ.setdefault("FLAGS_enable_mkldnn", "0")
os.environ.setdefault("FLAGS_enable_pir_api", "0")

CONFIG_PATH = Path(__file__).with_name("config.json")


def load_config() -> dict[str, Any]:
    if not CONFIG_PATH.exists():
        return {}
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as config_file:
            config = json.load(config_file)
        return config if isinstance(config, dict) else {}
    except Exception:
        return {}


OCR_CONFIG = load_config()


def configured_string(config_key: str, environment_key: str, default: str = "") -> str:
    environment_value = os.environ.get(environment_key, "").strip()
    if environment_value:
        return environment_value
    value = OCR_CONFIG.get(config_key)
    if value is None:
        return default
    return str(value).strip()


def configured_bool(config_key: str, environment_key: str, default: bool = False) -> bool:
    environment_value = os.environ.get(environment_key, "").strip().lower()
    if environment_value:
        return environment_value in {"1", "true", "yes", "on"}
    value = OCR_CONFIG.get(config_key)
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "on"}
    return default


def log_progress(message: str) -> None:
    print(f"[ocr_preview] {message}", flush=True)


def write_output(
    path: str,
    title: str,
    text: str,
    summary: str | None = None,
    operations: list[dict[str, Any]] | None = None,
) -> None:
    payload: dict[str, Any] = {
        "apiVersion": 1,
        "summary": summary or text.splitlines()[0] if text else title,
        "result": {
            "type": "message",
            "title": title,
            "text": text,
        },
    }
    if operations is not None:
        payload["operations"] = operations

    with open(path, "w", encoding="utf-8") as output_file:
        json.dump(payload, output_file, ensure_ascii=False, indent=2)


def selected_target(payload: dict[str, Any]) -> tuple[str | None, str, dict[str, Any] | None]:
    context = payload.get("context", {})
    current_page = context.get("currentPage", {})
    selection = payload.get("selection", {})

    if selection.get("hasSelection"):
        return selection.get("imagePath"), "selection", selection.get("rect", {})

    if current_page.get("hasPage"):
        return current_page.get("imagePath"), "current page", None

    return None, "none", None


def crop_selection(image_path: str, rect: dict[str, Any], temp_dir: str) -> tuple[str, str]:
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is required to crop the selected region. Install requirements.txt first.") from error

    image = Image.open(image_path)
    width, height = image.size
    left = max(0.0, min(1.0, float(rect.get("left", rect.get("x", 0.0)))))
    top = max(0.0, min(1.0, float(rect.get("top", rect.get("y", 0.0)))))
    right = max(0.0, min(1.0, float(rect.get("right", left + rect.get("width", 0.0)))))
    bottom = max(0.0, min(1.0, float(rect.get("bottom", top + rect.get("height", 0.0)))))

    x1 = min(width - 1, max(0, int(round(min(left, right) * width))))
    y1 = min(height - 1, max(0, int(round(min(top, bottom) * height))))
    x2 = min(width, max(x1 + 1, int(round(max(left, right) * width))))
    y2 = min(height, max(y1 + 1, int(round(max(top, bottom) * height))))

    crop_path = str(Path(temp_dir) / "labelminus_ocr_selection.png")
    log_progress(f"Cropping selected region from {image_path}")
    image.crop((x1, y1, x2, y2)).save(crop_path)
    return crop_path, f"selection pixels: left={x1}, top={y1}, right={x2}, bottom={y2}"


def create_paddle_ocr() -> Any:
    try:
        from paddleocr import PaddleOCR
    except ImportError as error:
        raise RuntimeError("PaddleOCR is not installed. Run `pip install -r requirements.txt` in this script directory.") from error

    lang = configured_string("language", "LABELMINUS_OCR_LANG", "japan")
    device = configured_string("device", "LABELMINUS_OCR_DEVICE", "cpu")
    log_progress(f"Initializing PaddleOCR language={lang} device={device}")
    candidates = [
        {
            "lang": lang,
            "device": device,
            "enable_mkldnn": False,
            "use_doc_orientation_classify": False,
            "use_doc_unwarping": False,
            "use_textline_orientation": False,
        },
        {"lang": lang, "device": device, "enable_mkldnn": False},
        {"lang": lang, "device": device},
        {"lang": lang},
        {},
    ]
    last_error: Exception | None = None
    for kwargs in candidates:
        try:
            return PaddleOCR(**kwargs)
        except Exception as error:  # PaddleOCR has changed constructor names across releases.
            last_error = error
    raise RuntimeError(f"Failed to initialize PaddleOCR: {last_error}")


def run_paddle(image_path: str) -> Any:
    ocr = create_paddle_ocr()
    log_progress(f"Running PaddleOCR on {image_path}")
    if hasattr(ocr, "predict"):
        return ocr.predict(image_path)
    return ocr.ocr(image_path)


def runtime_versions() -> str:
    versions: list[str] = []
    try:
        import paddle

        versions.append(f"paddlepaddle={paddle.__version__}")
    except Exception:
        versions.append("paddlepaddle=unavailable")
    try:
        import paddleocr

        versions.append(f"paddleocr={getattr(paddleocr, '__version__', 'unknown')}")
    except Exception:
        versions.append("paddleocr=unavailable")
    try:
        import manga_ocr

        versions.append(f"manga-ocr={getattr(manga_ocr, '__version__', 'unknown')}")
    except Exception:
        versions.append("manga-ocr=unavailable")
    try:
        import transformers

        versions.append(f"transformers={transformers.__version__}")
    except Exception:
        versions.append("transformers=unavailable")
    return ", ".join(versions)


def resolve_manga_ocr_model_path() -> str:
    configured_path = configured_string("mangaModelPath", "LABELMINUS_MANGA_OCR_MODEL")
    if configured_path:
        return configured_path

    try:
        from huggingface_hub import snapshot_download

        return snapshot_download("kha-white/manga-ocr-base", local_files_only=True)
    except Exception:
        return "kha-white/manga-ocr-base"


def troubleshooting_text(engine: str) -> str:
    if engine in {"manga", "manga-with-paddle", "manga_with_paddle"}:
        return (
            "For manga-ocr mode, make sure the HuggingFace model kha-white/manga-ocr-base can be downloaded "
            "or is already cached. This script tries to use the local HuggingFace snapshot first; you can also "
            "set LABELMINUS_MANGA_OCR_MODEL to an explicit local model directory. manga-ocr 0.1.x expects the "
            "Transformers 4.x API, so install this script's requirements again if your environment has "
            "Transformers 5.x:\n"
            "pip install -r scripts/official/ocr_preview/requirements.txt\n\n"
            "Optional environment variables:\n"
            "LABELMINUS_OCR_LANG=japan\n"
            "LABELMINUS_OCR_DEVICE=cpu\n"
            "LABELMINUS_OCR_ENGINE=paddle or manga-with-paddle\n"
            "LABELMINUS_MANGA_OCR_MODEL=/path/to/kha-white/manga-ocr-base"
        )

    return (
        "If imports succeeded, this is usually a PaddleOCR/Paddle runtime backend issue rather than a missing package. "
        "This script disables oneDNN/MKLDNN by default for the CPU path; if the problem persists, try a different "
        "PaddleOCR/Paddle version combination.\n\n"
        "Optional environment variables:\n"
        "LABELMINUS_OCR_LANG=japan\n"
        "LABELMINUS_OCR_DEVICE=cpu\n"
        "LABELMINUS_OCR_ENGINE=paddle or manga-with-paddle"
    )


def box_bounds(box: Any) -> tuple[int, int, int, int] | None:
    try:
        if hasattr(box, "tolist"):
            return box_bounds(box.tolist())
        if isinstance(box, dict):
            if "box" in box:
                return box_bounds(box["box"])
            if all(key in box for key in ("x", "y", "width", "height")):
                x = int(round(float(box["x"])))
                y = int(round(float(box["y"])))
                return x, y, x + int(round(float(box["width"]))), y + int(round(float(box["height"])))
        if isinstance(box, (list, tuple)) and len(box) == 4 and all(isinstance(value, (int, float)) for value in box):
            x1, y1, x2, y2 = [int(round(float(value))) for value in box]
            return min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2)
        if isinstance(box, (list, tuple)) and len(box) >= 4:
            xs = [float(point[0]) for point in box]
            ys = [float(point[1]) for point in box]
            return int(min(xs)), int(min(ys)), int(max(xs)), int(max(ys))
    except Exception:
        return None
    return None


MIN_CONFIDENCE = 0.6
MIN_REGION_AREA_RATIO = 0.00002
MIN_REGION_SIDE = 4
MERGE_PADDING_RATIO = 0.008
MERGE_MAX_DISTANCE = 16.0
MERGE_DISTANCE_SCALE = 0.6
DEDUP_DISTANCE_RATIO = 0.002


def image_size(image_path: str) -> tuple[int, int]:
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is required to inspect image size. Install requirements.txt first.") from error

    with Image.open(image_path) as image:
        return image.size


def region_bounds(region: dict[str, Any]) -> tuple[int, int, int, int] | None:
    bounds = region.get("bounds")
    if bounds is not None:
        return box_bounds(bounds)
    return box_bounds(region.get("box"))


def set_region_bounds(region: dict[str, Any], bounds: tuple[int, int, int, int]) -> dict[str, Any]:
    copied = dict(region)
    copied["bounds"] = [bounds[0], bounds[1], bounds[2], bounds[3]]
    copied["box"] = copied["bounds"]
    return copied


def bounds_width(bounds: tuple[int, int, int, int]) -> int:
    return max(0, bounds[2] - bounds[0])


def bounds_height(bounds: tuple[int, int, int, int]) -> int:
    return max(0, bounds[3] - bounds[1])


def bounds_area(bounds: tuple[int, int, int, int]) -> int:
    return bounds_width(bounds) * bounds_height(bounds)


def bounds_center(bounds: tuple[int, int, int, int]) -> tuple[float, float]:
    return (bounds[0] + bounds[2]) / 2.0, (bounds[1] + bounds[3]) / 2.0


def union_bounds(first: tuple[int, int, int, int], second: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    return min(first[0], second[0]), min(first[1], second[1]), max(first[2], second[2]), max(first[3], second[3])


def normalize_regions(raw_regions: list[dict[str, Any]], width: int, height: int) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    for region in raw_regions:
        bounds = region_bounds(region)
        if bounds is None:
            continue
        left, top, right, bottom = bounds
        left = max(0, min(width - 1, left))
        top = max(0, min(height - 1, top))
        right = max(left + 1, min(width, right))
        bottom = max(top + 1, min(height, bottom))

        score = region.get("score")
        try:
            confidence = float(score)
        except (TypeError, ValueError):
            confidence = 1.0
        if confidence < MIN_CONFIDENCE:
            continue

        text = str(region.get("text", "")).strip()
        normalized.append({"text": text, "score": confidence, "box": [left, top, right, bottom]})
    return normalized


def is_useful_region(region: dict[str, Any], image_width: int, image_height: int) -> bool:
    bounds = region_bounds(region)
    if bounds is None:
        return False
    if bounds_width(bounds) < MIN_REGION_SIDE or bounds_height(bounds) < MIN_REGION_SIDE:
        return False
    return bounds_area(bounds) >= image_width * image_height * MIN_REGION_AREA_RATIO


def is_vertical_layout(regions: list[dict[str, Any]]) -> bool:
    vertical_count = 0
    horizontal_count = 0
    vertical_score = 0.0
    horizontal_score = 0.0

    for region in regions:
        bounds = region_bounds(region)
        if bounds is None:
            continue
        width = max(1, bounds_width(bounds))
        height = max(1, bounds_height(bounds))
        vertical_aspect = height / width
        horizontal_aspect = width / height
        if vertical_aspect >= 1.8:
            vertical_count += 1
            vertical_score += vertical_aspect
        if horizontal_aspect >= 1.8:
            horizontal_count += 1
            horizontal_score += horizontal_aspect

    region_count = len(regions)
    if region_count == 0:
        return False
    if region_count <= 2:
        return vertical_count == region_count and vertical_score >= horizontal_score * 1.6
    return vertical_count >= max(2, horizontal_count + 1) and vertical_score >= horizontal_score * 1.25


def reading_order_key(region: dict[str, Any], vertical: bool, right_to_left: bool) -> tuple[float, float]:
    bounds = region_bounds(region)
    if bounds is None:
        return 0.0, 0.0
    center_x, center_y = bounds_center(bounds)
    if vertical:
        return (-center_x if right_to_left else center_x), center_y
    return center_y, (-center_x if right_to_left else center_x)


def ordered_pair(first: dict[str, Any], second: dict[str, Any], vertical: bool, right_to_left: bool) -> tuple[dict[str, Any], dict[str, Any]]:
    return tuple(sorted([first, second], key=lambda region: reading_order_key(region, vertical, right_to_left)))  # type: ignore[return-value]


def merge_text(first: str, second: str) -> str:
    if not first:
        return second
    if not second:
        return first
    return first + second


def expanded_intersects(first: tuple[int, int, int, int], second: tuple[int, int, int, int], padding: float) -> bool:
    return not (
        first[2] + padding < second[0]
        or second[2] + padding < first[0]
        or first[3] + padding < second[1]
        or second[3] + padding < first[1]
    )


def should_merge(first: dict[str, Any], second: dict[str, Any], image_width: int, image_height: int) -> bool:
    first_bounds = region_bounds(first)
    second_bounds = region_bounds(second)
    if first_bounds is None or second_bounds is None:
        return False

    padding = max(image_width, image_height) * MERGE_PADDING_RATIO
    x_overlap = min(first_bounds[2], second_bounds[2]) - max(first_bounds[0], second_bounds[0])
    y_overlap = min(first_bounds[3], second_bounds[3]) - max(first_bounds[1], second_bounds[1])
    if expanded_intersects(first_bounds, second_bounds, padding) and (
        x_overlap > -min(bounds_width(first_bounds), bounds_width(second_bounds)) * 0.5
        or y_overlap > -min(bounds_height(first_bounds), bounds_height(second_bounds)) * 0.5
    ):
        return True

    first_center = bounds_center(first_bounds)
    second_center = bounds_center(second_bounds)
    dx = first_center[0] - second_center[0]
    dy = first_center[1] - second_center[1]
    distance = (dx * dx + dy * dy) ** 0.5
    average_dimension = (
        max(bounds_width(first_bounds), bounds_height(first_bounds))
        + max(bounds_width(second_bounds), bounds_height(second_bounds))
    ) / 2.0
    return distance <= max(MERGE_MAX_DISTANCE, average_dimension * MERGE_DISTANCE_SCALE)


def merge_region_pair(first: dict[str, Any], second: dict[str, Any], vertical: bool, right_to_left: bool) -> dict[str, Any]:
    first_ordered, second_ordered = ordered_pair(first, second, vertical, right_to_left)
    first_bounds = region_bounds(first)
    second_bounds = region_bounds(second)
    if first_bounds is None or second_bounds is None:
        return dict(first)
    confidence = min(float(first.get("score", 1.0)), float(second.get("score", 1.0)))
    return {
        "text": merge_text(str(first_ordered.get("text", "")), str(second_ordered.get("text", ""))),
        "score": confidence,
        "box": list(union_bounds(first_bounds, second_bounds)),
    }


def build_text_blocks(
    regions: list[dict[str, Any]], image_width: int, image_height: int, vertical: bool, right_to_left: bool
) -> list[dict[str, Any]]:
    blocks = [region for region in regions if is_useful_region(region, image_width, image_height)]
    if len(blocks) <= 1:
        return blocks

    changed = True
    while changed:
        changed = False
        for first_index in range(len(blocks)):
            merged_index = -1
            for second_index in range(first_index + 1, len(blocks)):
                if should_merge(blocks[first_index], blocks[second_index], image_width, image_height):
                    merged_index = second_index
                    break

            if merged_index >= 0:
                merged = merge_region_pair(blocks[first_index], blocks[merged_index], vertical, right_to_left)
                blocks[first_index] = merged
                del blocks[merged_index]
                changed = True
                break

    return blocks


def deduplicate_regions(regions: list[dict[str, Any]], image_width: int, image_height: int) -> list[dict[str, Any]]:
    deduped: list[dict[str, Any]] = []
    distance_threshold = max(image_width, image_height) * DEDUP_DISTANCE_RATIO
    for region in regions:
        bounds = region_bounds(region)
        if bounds is None:
            continue
        center = bounds_center(bounds)
        duplicate = False
        for existing in deduped:
            existing_bounds = region_bounds(existing)
            if existing_bounds is None:
                continue
            existing_center = bounds_center(existing_bounds)
            dx = center[0] - existing_center[0]
            dy = center[1] - existing_center[1]
            if (dx * dx + dy * dy) ** 0.5 <= distance_threshold and str(region.get("text", "")) == str(
                existing.get("text", "")
            ):
                duplicate = True
                break
        if not duplicate:
            deduped.append(region)
    return deduped


def sort_regions(regions: list[dict[str, Any]], vertical: bool, right_to_left: bool) -> list[dict[str, Any]]:
    if not regions:
        return []

    if vertical:
        valid_regions = [region for region in regions if region_bounds(region) is not None]
        average_width = sum(bounds_width(region_bounds(region)) for region in valid_regions) / max(1, len(valid_regions))  # type: ignore[arg-type]
        tolerance = max(24.0, average_width * 0.8)
        columns: list[list[dict[str, Any]]] = []
        for region in sorted(valid_regions, key=lambda item: bounds_center(region_bounds(item))[0], reverse=right_to_left):  # type: ignore[arg-type]
            center_x = bounds_center(region_bounds(region))[0]  # type: ignore[arg-type]
            for column in columns:
                column_center = sum(bounds_center(region_bounds(item))[0] for item in column) / len(column)  # type: ignore[arg-type]
                if abs(center_x - column_center) <= tolerance:
                    column.append(region)
                    break
            else:
                columns.append([region])

        ordered: list[dict[str, Any]] = []
        for column in columns:
            ordered.extend(sorted(column, key=lambda item: bounds_center(region_bounds(item))[1]))  # type: ignore[arg-type]
        return ordered

    valid_regions = [region for region in regions if region_bounds(region) is not None]
    average_height = sum(bounds_height(region_bounds(region)) for region in valid_regions) / max(1, len(valid_regions))  # type: ignore[arg-type]
    tolerance = max(24.0, average_height * 0.8)
    rows: list[list[dict[str, Any]]] = []
    for region in sorted(valid_regions, key=lambda item: bounds_center(region_bounds(item))[1]):  # type: ignore[arg-type]
        center_y = bounds_center(region_bounds(region))[1]  # type: ignore[arg-type]
        for row in rows:
            row_center = sum(bounds_center(region_bounds(item))[1] for item in row) / len(row)  # type: ignore[arg-type]
            if abs(center_y - row_center) <= tolerance:
                row.append(region)
                break
        else:
            rows.append([region])

    ordered: list[dict[str, Any]] = []
    for row in rows:
        ordered.extend(sorted(row, key=lambda item: bounds_center(region_bounds(item))[0], reverse=right_to_left))  # type: ignore[arg-type]
    return ordered


def post_process_regions(
    regions: list[dict[str, Any]], image_width: int, image_height: int, already_merged: bool, right_to_left: bool
) -> list[dict[str, Any]]:
    vertical = is_vertical_layout(regions)
    blocks = regions if already_merged else build_text_blocks(regions, image_width, image_height, vertical, right_to_left)
    deduped = deduplicate_regions(blocks, image_width, image_height)
    return sort_regions(deduped, vertical, right_to_left)


def merge_all_regions(regions: list[dict[str, Any]], vertical: bool, right_to_left: bool) -> dict[str, Any] | None:
    if not regions:
        return None

    sorted_regions = sort_regions(regions, vertical, right_to_left)
    merged_bounds = region_bounds(sorted_regions[0])
    if merged_bounds is None:
        return None
    text = str(sorted_regions[0].get("text", ""))
    confidence = float(sorted_regions[0].get("score", 1.0))
    for region in sorted_regions[1:]:
        bounds = region_bounds(region)
        if bounds is None:
            continue
        merged_bounds = union_bounds(merged_bounds, bounds)
        text = merge_text(text, str(region.get("text", "")))
        confidence = min(confidence, float(region.get("score", 1.0)))
    return {"text": text, "score": confidence, "box": list(merged_bounds)}


def parse_paddle_regions(result: Any) -> list[dict[str, Any]]:
    regions: list[dict[str, Any]] = []

    def first_present(mapping: dict[str, Any], keys: tuple[str, ...]) -> Any:
        for key in keys:
            if key in mapping and mapping[key] is not None:
                return mapping[key]
        return []

    def visit(value: Any) -> None:
        if isinstance(value, dict):
            texts = value.get("rec_texts")
            if isinstance(texts, list):
                scores = first_present(value, ("rec_scores",))
                boxes = first_present(value, ("rec_boxes", "dt_polys", "rec_polys"))
                for index, text in enumerate(texts):
                    box = boxes[index] if index < len(boxes) else None
                    score = scores[index] if index < len(scores) else None
                    regions.append({"text": str(text), "score": score, "box": box})
                return
            for child in value.values():
                visit(child)
            return

        if isinstance(value, (list, tuple)):
            if (
                len(value) >= 2
                and isinstance(value[1], (list, tuple))
                and len(value[1]) >= 1
                and isinstance(value[1][0], str)
            ):
                score = value[1][1] if len(value[1]) > 1 else None
                regions.append({"text": value[1][0], "score": score, "box": value[0]})
                return
            for child in value:
                visit(child)

    visit(result)
    return regions


def run_manga_on_paddle_boxes(image_path: str, regions: list[dict[str, Any]], temp_dir: str) -> list[dict[str, Any]]:
    try:
        from PIL import Image
        from manga_ocr import MangaOcr
    except ImportError as error:
        raise RuntimeError(
            "manga-ocr mode requires Pillow and manga-ocr. Run `pip install -r requirements.txt` first."
        ) from error

    model_path = resolve_manga_ocr_model_path()
    log_progress(f"Initializing manga-ocr recognizer model={model_path}")
    recognizer = MangaOcr(model_path)
    image = Image.open(image_path)
    width, height = image.size
    recognized: list[dict[str, Any]] = []
    for index, region in enumerate(regions):
        log_progress(f"Recognizing manga region {index + 1}/{len(regions)}")
        bounds = box_bounds(region.get("box"))
        if bounds is None:
            continue
        left, top, right, bottom = bounds
        left = max(0, min(width - 1, left))
        top = max(0, min(height - 1, top))
        right = max(left + 1, min(width, right))
        bottom = max(top + 1, min(height, bottom))
        crop_path = str(Path(temp_dir) / f"manga_region_{index:04d}.png")
        image.crop((left, top, right, bottom)).save(crop_path)
        recognized.append({"text": recognizer(crop_path), "score": 1.0, "box": [left, top, right, bottom]})
    return recognized


def ocr_regions(image_path: str, engine: str, temp_dir: str) -> tuple[str, list[dict[str, Any]], int, int]:
    width, height = image_size(image_path)
    raw_regions = normalize_regions(parse_paddle_regions(run_paddle(image_path)), width, height)
    log_progress(f"PaddleOCR detected {len(raw_regions)} text region(s)")
    right_to_left = configured_bool("rightToLeft", "LABELMINUS_OCR_RIGHT_TO_LEFT")

    if engine in {"manga", "manga-with-paddle", "manga_with_paddle"}:
        detection_vertical = is_vertical_layout(raw_regions)
        detection_blocks = build_text_blocks(raw_regions, width, height, detection_vertical, True)
        log_progress(f"Merged PaddleOCR detections into {len(detection_blocks)} manga recognition block(s)")
        recognized_regions = run_manga_on_paddle_boxes(image_path, detection_blocks, temp_dir)
        processed_regions = post_process_regions(recognized_regions, width, height, already_merged=True, right_to_left=True)
        return "manga-ocr with PaddleOCR detection", processed_regions, width, height

    processed_regions = post_process_regions(raw_regions, width, height, already_merged=False, right_to_left=right_to_left)
    return "PaddleOCR", processed_regions, width, height


def format_regions(regions: list[dict[str, Any]], engine: str, target_description: str, image_path: str) -> str:
    if not regions:
        return f"OCR engine: {engine}\nTarget: {target_description}\nImage: {image_path}\n\nNo text regions were recognized."

    lines = [
        f"OCR engine: {engine}",
        f"Target: {target_description}",
        f"Image: {image_path}",
        f"Recognized regions: {len(regions)}",
        "",
    ]
    for index, region in enumerate(regions, start=1):
        score = region.get("score")
        score_text = ""
        if isinstance(score, (int, float)):
            score_text = f" ({score:.3f})"
        bounds = region_bounds(region)
        bounds_text = f" [{bounds[0]},{bounds[1]},{bounds[2]},{bounds[3]}]" if bounds else ""
        text = str(region.get("text", "")).strip()
        lines.append(f"#{index}{score_text}{bounds_text}: {text}")
    return "\n".join(lines)


def current_page_name(payload: dict[str, Any]) -> str:
    context = payload.get("context", {})
    current_page = context.get("currentPage", {})
    page_name = str(current_page.get("name", "")).strip()
    if page_name:
        return page_name
    project = payload.get("project", {})
    return str(project.get("currentPage", "")).strip()


def default_label_group(payload: dict[str, Any]) -> str:
    configured_group = configured_string("defaultGroup", "LABELMINUS_OCR_GROUP")
    if configured_group:
        return configured_group
    groups = payload.get("project", {}).get("groups", [])
    if "框内" in groups:
        return "框内"
    if groups:
        return str(groups[0])
    return "框内"


def operation_position_for_region(
    region: dict[str, Any],
    image_width: int,
    image_height: int,
    selection_rect: dict[str, Any] | None,
) -> tuple[float, float]:
    bounds = region_bounds(region)
    if bounds is None:
        return 0.0, 0.0

    if selection_rect:
        left = max(0.0, min(1.0, float(selection_rect.get("left", selection_rect.get("x", 0.0)))))
        top = max(0.0, min(1.0, float(selection_rect.get("top", selection_rect.get("y", 0.0)))))
        width = max(0.0, min(1.0, float(selection_rect.get("width", 0.0))))
        height = max(0.0, min(1.0, float(selection_rect.get("height", 0.0))))
        return (
            max(0.0, min(1.0, left + (bounds[2] / max(1, image_width)) * width)),
            max(0.0, min(1.0, top + (bounds[1] / max(1, image_height)) * height)),
        )

    return (
        max(0.0, min(1.0, bounds[2] / max(1, image_width))),
        max(0.0, min(1.0, bounds[1] / max(1, image_height))),
    )


def operations_for_regions(
    payload: dict[str, Any],
    regions: list[dict[str, Any]],
    image_width: int,
    image_height: int,
    selection_rect: dict[str, Any] | None,
) -> list[dict[str, Any]]:
    page_name = current_page_name(payload)
    group = default_label_group(payload)
    if not page_name:
        return []

    operation_regions = regions
    if selection_rect:
        merged_region = merge_all_regions(regions, is_vertical_layout(regions), True)
        operation_regions = [merged_region] if merged_region is not None else []

    operations: list[dict[str, Any]] = []
    for index, region in enumerate(operation_regions, start=1):
        text = str(region.get("text", "")).strip() or f"Label{index}"
        x, y = operation_position_for_region(region, image_width, image_height, selection_rect)
        operations.append(
            {
                "type": "addLabel",
                "page": page_name,
                "group": group,
                "text": text,
                "x": x,
                "y": y,
            }
        )
    return operations


def main() -> None:
    parser = argparse.ArgumentParser(description="Preview OCR results for the current LabelMinus page or selection.")
    parser.add_argument("--input", required=True, help="Path to the LabelMinus automation input JSON.")
    parser.add_argument("--output", required=True, help="Path to write the automation output JSON.")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as input_file:
        payload = json.load(input_file)

    image_path, target_kind, selection_rect = selected_target(payload)
    if not image_path:
        write_output(args.output, "OCR Preview", "Open a project page before running OCR.")
        return
    if not Path(image_path).exists():
        write_output(args.output, "OCR Preview", f"Image does not exist:\n{image_path}")
        return

    engine = configured_string("engine", "LABELMINUS_OCR_ENGINE", "paddle").lower()
    log_progress(f"Starting OCR preview engine={engine}")
    with tempfile.TemporaryDirectory(prefix="labelminus_ocr_") as temp_dir:
        target_path = image_path
        target_description = target_kind
        if selection_rect:
            try:
                target_path, crop_detail = crop_selection(image_path, selection_rect, temp_dir)
                target_description = f"selection ({crop_detail})"
            except Exception as error:
                write_output(args.output, "OCR Preview", f"Failed to crop selected region:\n{error}")
                return

        try:
            engine_name, regions, width, height = ocr_regions(target_path, engine, temp_dir)
            result_text = format_regions(regions, engine_name, target_description, target_path)
            action = configured_string("action", "LABELMINUS_OCR_ACTION", "preview").lower()
            operations: list[dict[str, Any]] | None = None
            if action in {"add-labels", "add_labels"}:
                operations = operations_for_regions(payload, regions, width, height, selection_rect)
                result_text = (
                    result_text
                    + "\n\n"
                    + f"Prepared {len(operations)} label operation(s). "
                    + "Selection OCR is merged into one LabelMinus label; full-page OCR creates one label per text block."
                )
        except Exception as error:
            result_text = (
                f"OCR failed:\n{error}\n\n"
                f"Runtime: {runtime_versions()}\n\n"
                f"{troubleshooting_text(engine)}"
            )
            operations = None

    title = "OCR Add Labels" if operations is not None else "OCR Preview"
    write_output(args.output, title, result_text, operations=operations)


if __name__ == "__main__":
    main()
