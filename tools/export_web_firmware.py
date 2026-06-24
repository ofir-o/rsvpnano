#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEB_FIRMWARE_DIR = ROOT / "web" / "firmware"
BOOT_APP0_GLOB = "framework-arduinoespressif32*/tools/partitions/boot_app0.bin"

# The NVS partition holds user settings that must survive a USB re-flash: bookmarks, reading
# positions, theme, and pet state. (See partitions_16MB_ffat.csv — nvs @ 0x9000, size 0x5000.)
# A single contiguous merged image would 0xFF-fill across this region and wipe it on every flash,
# so the merged image is split into two parts that straddle NVS without touching it. ESP Web Tools
# flashes each part at its own offset, leaving 0x9000..0xE000 (NVS) exactly as the device left it.
NVS_OFFSET = 0x9000
NVS_END = 0xE000

FLASH_EXPORTS = (
    {
        "env": "waveshare_esp32s3",
        "binary": "rsvp-nano.bin",
        "manifest": "manifest.json",
        "label": "RSVP Nano Touch LCD 3.49 rev1 firmware",
    },
    {
        "env": "waveshare_esp32s3_rev2",
        "binary": "rsvp-nano-rev2.bin",
        "manifest": "manifest-rev2.json",
        "label": "RSVP Nano Touch LCD 3.49 rev2/GPIO42 firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_18",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.8.bin",
        "manifest": "manifest-esp32-s3-touch-amoled-1.8.json",
        "label": "RSVP Nano Touch AMOLED 1.8 V1 firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_18_v2",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.8-v2.bin",
        "manifest": "manifest-esp32-s3-touch-amoled-1.8-v2.json",
        "label": "RSVP Nano Touch AMOLED 1.8 V2 Test firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_175",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.75.bin",
        "manifest": "manifest-esp32-s3-touch-amoled-1.75.json",
        "label": "RSVP Nano Touch AMOLED 1.75 firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_216",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.16.bin",
        "manifest": "manifest-esp32-s3-touch-amoled-2.16.json",
        "label": "RSVP Nano Touch AMOLED 2.16 firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_241",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.41.bin",
        "manifest": "manifest-esp32-s3-touch-amoled-2.41.json",
        "label": "RSVP Nano Touch AMOLED 2.41 firmware",
    },
)

OTA_EXPORTS = (
    {
        "env": "waveshare_esp32s3",
        "binary": "rsvp-nano-ota.bin",
        "label": "RSVP Nano Touch LCD 3.49 OTA firmware (legacy asset)",
    },
    {
        "env": "waveshare_esp32s3",
        "binary": "rsvp-nano-esp32-s3-touch-lcd-3.49-ota.bin",
        "label": "RSVP Nano Touch LCD 3.49 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_rev2",
        "binary": "rsvp-nano-rev2-ota.bin",
        "label": "RSVP Nano Touch LCD 3.49 rev2 OTA firmware (legacy asset)",
    },
    {
        "env": "waveshare_esp32s3_rev2",
        "binary": "rsvp-nano-esp32-s3-touch-lcd-3.49-rev2-ota.bin",
        "label": "RSVP Nano Touch LCD 3.49 rev2/GPIO42 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_18",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.8-ota.bin",
        "label": "RSVP Nano Touch AMOLED 1.8 V1 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_18_v2",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.8-v2-ota.bin",
        "label": "RSVP Nano Touch AMOLED 1.8 V2 Test OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_175",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-1.75-ota.bin",
        "label": "RSVP Nano Touch AMOLED 1.75 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_216",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.16-ota.bin",
        "label": "RSVP Nano Touch AMOLED 2.16 OTA firmware",
    },
    {
        "env": "waveshare_esp32s3_touch_amoled_241",
        "binary": "rsvp-nano-esp32-s3-touch-amoled-2.41-ota.bin",
        "label": "RSVP Nano Touch AMOLED 2.41 OTA firmware",
    },
)


def run(command: list[str], version: str | None = None) -> None:
    print("+", " ".join(command))
    env = os.environ.copy()
    env.setdefault("PLATFORMIO_SETTING_ENABLE_TELEMETRY", "No")
    if version:
        env["RSVP_FIRMWARE_VERSION"] = version
    subprocess.run(command, cwd=ROOT, check=True, env=env)


def pio_command() -> str:
    local = Path.home() / ".platformio" / "penv" / "bin" / "pio"
    if local.exists():
        return str(local)

    found = shutil.which("pio")
    if found:
        return found

    raise SystemExit("PlatformIO Core was not found. Install it or activate the PlatformIO env.")


def git_version() -> str:
    try:
        value = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"], cwd=ROOT, text=True
        ).strip()
        return value or "dev"
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "dev"


def find_boot_app0() -> Path:
    search_roots = [
        ROOT / ".pio" / "packages",
        Path.home() / ".platformio" / "packages",
    ]
    candidates: list[Path] = []
    for root in search_roots:
        candidates.extend(root.glob(BOOT_APP0_GLOB))

    if not candidates:
        raise SystemExit("Could not find Arduino ESP32 boot_app0.bin after PlatformIO build.")

    return sorted(candidates)[-1]


def load_flash_parts(env: str) -> list[tuple[int, Path]]:
    build_dir = ROOT / ".pio" / "build" / env
    parts: list[tuple[int, Path]] = [
        (0x0000, build_dir / "bootloader.bin"),
        (0x8000, build_dir / "partitions.bin"),
        (0xE000, find_boot_app0()),
        (0x10000, build_dir / "firmware.bin"),
    ]

    for _, path in parts:
        if not path.exists():
            raise SystemExit(f"Missing flash part for {env}: {path}")

    return sorted(parts, key=lambda item: item[0])


def _merge_segment(segment: list[tuple[int, Path]], output: Path) -> int:
    """Write one contiguous flash segment, 0xFF-filling gaps relative to its own start offset.

    Returns the flash offset the segment must be written at.
    """
    start = segment[0][0]
    cursor = start
    with output.open("wb") as merged:
        for offset, path in segment:
            if offset < cursor:
                raise SystemExit(f"Overlapping flash part: {path}")

            gap = offset - cursor
            if gap > 0:
                merged.write(b"\xFF" * gap)
                cursor = offset

            data = path.read_bytes()
            merged.write(data)
            cursor += len(data)
    return start


def merge_firmware(env: str, output: Path) -> list[tuple[int, str]]:
    """Build the web-flasher image(s) for an env, skipping the NVS region.

    Returns the manifest part list as (offset, filename) tuples.
    """
    parts = load_flash_parts(env)
    output.parent.mkdir(parents=True, exist_ok=True)

    inside_nvs = [path for offset, path in parts if NVS_OFFSET <= offset < NVS_END]
    if inside_nvs:
        raise SystemExit(f"Flash part lands inside the preserved NVS region for {env}: {inside_nvs}")

    low = [item for item in parts if item[0] < NVS_OFFSET]
    high = [item for item in parts if item[0] >= NVS_END]

    manifest_parts: list[tuple[int, str]] = []
    if low:
        low_start = _merge_segment(low, output)
        manifest_parts.append((low_start, output.name))
    if high:
        high_output = output.with_name(f"{output.stem}-app{output.suffix}")
        high_start = _merge_segment(high, high_output)
        manifest_parts.append((high_start, high_output.name))

    return manifest_parts


def export_ota_binary(env: str, output: Path) -> None:
    firmware_path = ROOT / ".pio" / "build" / env / "firmware.bin"
    if not firmware_path.exists():
        raise SystemExit(f"Missing OTA app binary for {env}: {firmware_path}")

    shutil.copy2(firmware_path, output)


def update_manifest(path: Path, version: str, manifest_parts: list[tuple[int, str]]) -> None:
    manifest = json.loads(path.read_text())
    manifest["version"] = version
    builds = manifest.get("builds")
    if builds and manifest_parts:
        builds[0]["parts"] = [{"path": name, "offset": offset} for offset, name in manifest_parts]
    path.write_text(json.dumps(manifest, indent=2) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build merged binaries for the web flasher.")
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Use existing .pio build outputs instead of running PlatformIO first.",
    )
    parser.add_argument("--version", default=git_version(), help="Version string for manifests.")
    parser.add_argument(
        "--only",
        action="append",
        dest="only_envs",
        help="Limit build/export to these PlatformIO env names. Repeatable. "
        "Useful for forks that only ship one board.",
    )
    args = parser.parse_args()

    flash_exports = FLASH_EXPORTS
    ota_exports = OTA_EXPORTS
    if args.only_envs:
        only = set(args.only_envs)
        flash_exports = tuple(export for export in FLASH_EXPORTS if export["env"] in only)
        ota_exports = tuple(export for export in OTA_EXPORTS if export["env"] in only)
        if not flash_exports and not ota_exports:
            known = sorted({export["env"] for export in FLASH_EXPORTS})
            raise SystemExit(f"--only matched no known envs: {sorted(only)}; known: {known}")

    pio = None if args.skip_build else pio_command()
    WEB_FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)

    if not args.skip_build:
        required_envs = sorted(
            {export["env"] for export in flash_exports}
            | {export["env"] for export in ota_exports}
        )
        for env in required_envs:
            assert pio is not None
            run([pio, "run", "-e", env], args.version)

    for export in flash_exports:
        output = WEB_FIRMWARE_DIR / export["binary"]
        print(f"Exporting {export['label']} -> {output}")
        manifest_parts = merge_firmware(export["env"], output)
        update_manifest(WEB_FIRMWARE_DIR / export["manifest"], args.version, manifest_parts)

    for export in ota_exports:
        ota_output = WEB_FIRMWARE_DIR / export["binary"]
        print(f"Exporting {export['label']} -> {ota_output}")
        export_ota_binary(export["env"], ota_output)

    print(f"Web firmware exported to {WEB_FIRMWARE_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
