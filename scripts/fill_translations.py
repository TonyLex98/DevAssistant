#!/usr/bin/env python3
"""Fill a Qt Linguist .ts file from simple JSON maps of "English source": "translation".

Used by the build after `lupdate` has extracted the strings:

    lupdate DevAssistantQt.pro -ts translations/devassistant_ru.ts -no-obsolete -locations none
    python scripts/fill_translations.py translations/devassistant_ru.ts translations/ru

Every message whose source text is found in one of the JSON files in the folder gets that
translation, in every context it appears in. Plural messages (%n) get the same text in each
form. Messages without a translation stay "unfinished" and are shown in English.
"""
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def load_maps(folder: Path) -> dict:
    merged = {}
    for f in sorted(folder.glob("*.json")):
        merged.update(json.loads(f.read_text(encoding="utf-8")))
    return merged


def main() -> int:
    ts_path, folder = Path(sys.argv[1]), Path(sys.argv[2])
    translations = load_maps(folder)
    tree = ET.parse(ts_path)
    root = tree.getroot()
    root.set("language", "ru_RU")

    total = filled = 0
    for message in root.iter("message"):
        total += 1
        source = message.findtext("source") or ""
        target = translations.get(source)
        translation = message.find("translation")
        if translation is None:
            translation = ET.SubElement(message, "translation")
        if target is None:
            continue
        filled += 1
        translation.attrib.pop("type", None)
        if message.get("numerus") == "yes":
            for form in list(translation):
                translation.remove(form)
            translation.text = None
            for _ in range(3):  # Russian has three plural forms
                ET.SubElement(translation, "numerusform").text = target
        else:
            translation.text = target

    tree.write(ts_path, encoding="utf-8", xml_declaration=True)
    print(f"{ts_path}: {filled} of {total} messages translated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
