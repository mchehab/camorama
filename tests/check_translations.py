#!/usr/bin/env python3
"""Report translation coverage against the current gettext template."""

import argparse
import glob
import json
import os
import subprocess
import sys
from typing import Dict, List, Optional, Sequence, Tuple

try:
    import polib
except ImportError as exc:
    raise SystemExit("check_translations.py requires python3-polib") from exc


COMPLETENESS_THRESHOLD = 58

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PO_DIR = os.path.join(ROOT, "po")
TEMPLATE = os.path.join(PO_DIR, "camorama.pot")
GSETTINGS_SOURCES = ["data/org.gnome.camorama.gschema.xml.in"]


class AsciiTable:
    def __init__(self, headers: Sequence[str], rows: Sequence[Sequence[str]]) -> None:
        self.headers = list(headers)
        self.rows = [list(row) for row in rows]
        if any(len(row) != len(self.headers) for row in self.rows):
            raise ValueError("Every table row must have one value per header")
        self.widths = [
            max([len(self.headers[index])] + [len(row[index]) for row in self.rows])
            for index in range(len(self.headers))
        ]

    def render(self) -> str:
        border = "+" + "+".join("-" * (width + 2) for width in self.widths) + "+"

        def format_row(row: Sequence[str]) -> str:
            cells = [f" {value:<{self.widths[index]}} " for index, value in enumerate(row)]
            return "|" + "|".join(cells) + "|"

        lines = [border, format_row(self.headers), border]
        lines.extend(format_row(row) for row in self.rows)
        lines.append(border)
        return "\n".join(lines)


def format_table(headers: Sequence[str], rows: Sequence[Sequence[str]]) -> str:
    return AsciiTable(headers, rows).render()


def identity(entry: polib.POEntry) -> Tuple[Optional[str], str, Optional[str]]:
    return entry.msgctxt, entry.msgid, entry.msgid_plural


def message_group(entry: polib.POEntry) -> str:
    if any(location[0] in GSETTINGS_SOURCES for location in entry.occurrences):
        return "GSettings"
    return "Camorama UI"


def translation_text(entry: polib.POEntry) -> str:
    if entry.msgid_plural:
        forms = [f"{index}={value}" for index, value in sorted(entry.msgstr_plural.items())]
        return "{" + ", ".join(forms) + "}"
    return entry.msgstr


def entry_status(entry: Optional[polib.POEntry]) -> str:
    if entry is None:
        return "MISSING"
    if "fuzzy" in entry.flags:
        return "FUZZY"
    if entry.msgid_plural:
        if any(not value for value in entry.msgstr_plural.values()):
            return "UNTRANSLATED"
    elif not entry.msgstr:
        return "UNTRANSLATED"
    return "TRANSLATED"


def group_summary(
    entries: Sequence[polib.POEntry], counts: Dict[str, int], obsolete_count: int
) -> str:
    total = len(entries)
    translated = counts["TRANSLATED"]
    percent = 100 * translated / total if total else 100
    missing = counts["UNTRANSLATED"] + counts["MISSING"]
    result = f"{percent:.0f}% ({translated}/{total})"
    if missing:
        result += f", {missing} missing"
    if counts["FUZZY"]:
        result += f", {counts['FUZZY']} fuzzy"
    if obsolete_count:
        result += f", {obsolete_count} obsolete"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(
        description="List current gettext messages and obsolete catalog entries."
    )
    parser.add_argument(
        "-s", "--summary",
        action="store_true",
        help="print only the aggregate translation summary",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="list each current message and obsolete catalog entry",
    )
    parser.add_argument(
        "-l", "--locale",
        nargs="+",
        action="extend",
        help="limit output to one or more locales (for example -l pt zh_CN)",
    )
    args = parser.parse_args()

    build_dir = os.environ.get("CAMORAMA_BUILD_DIR")
    if not build_dir:
        candidate = os.path.join(ROOT, "build")
        if os.path.exists(os.path.join(candidate, "build.ninja")):
            build_dir = candidate
    if build_dir:
        result = subprocess.run(
            ["ninja", "-C", build_dir, "camorama-pot"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if result.returncode:
            sys.stderr.write(result.stdout)
            raise SystemExit(result.returncode)

    if not os.path.exists(TEMPLATE):
        raise SystemExit(f"Missing gettext template: {TEMPLATE}; run `ninja -C build camorama-pot` first")

    template = polib.pofile(TEMPLATE)
    source_entries = [entry for entry in template if entry.msgid and not entry.obsolete]
    source_by_key = {identity(entry): entry for entry in source_entries}
    grouped_entries = {
        group: [entry for entry in source_entries if message_group(entry) == group]
        for group in ("Camorama UI", "GSettings")
    }
    locales = sorted(glob.glob(os.path.join(PO_DIR, "*.po")))
    if args.locale:
        requested = set(args.locale)
        locales = [path for path in locales if os.path.splitext(os.path.basename(path))[0] in requested]
        absent = requested - {os.path.splitext(os.path.basename(path))[0] for path in locales}
        if absent:
            raise SystemExit("Unknown locale(s): " + ", ".join(sorted(absent)))

    if args.verbose:
        print(
            f"Current template: {os.path.relpath(TEMPLATE, ROOT)} "
            f"({len(source_entries)} messages)"
        )
        print("Statuses: TRANSLATED, FUZZY, UNTRANSLATED, MISSING")
    summary_rows = []
    ui_over_threshold_count = 0
    gsettings_over_threshold_count = 0

    for path in locales:
        locale = os.path.splitext(os.path.basename(path))[0]
        catalog = polib.pofile(path)
        active_by_key = {
            identity(entry): entry
            for entry in catalog
            if entry.msgid and not entry.obsolete
        }
        obsolete = [
            entry
            for entry in catalog
            if entry.msgid and identity(entry) not in source_by_key
        ]
        obsolete_counts = {group: 0 for group in grouped_entries}
        for entry in obsolete:
            obsolete_counts[message_group(entry)] += 1
        group_counts = {}
        for group, entries in grouped_entries.items():
            counts = {
                status: sum(
                    entry_status(active_by_key.get(identity(source))) == status
                    for source in entries
                )
                for status in ("TRANSLATED", "FUZZY", "UNTRANSLATED", "MISSING")
            }
            group_counts[group] = counts
        ui_counts = group_counts["Camorama UI"]
        ui_total = len(grouped_entries["Camorama UI"])
        ui_percent = 100 * ui_counts["TRANSLATED"] / ui_total if ui_total else 100
        if ui_percent > COMPLETENESS_THRESHOLD:
            ui_over_threshold_count += 1
        gsettings_counts = group_counts["GSettings"]
        gsettings_total = len(grouped_entries["GSettings"])
        gsettings_percent = (
            100 * gsettings_counts["TRANSLATED"] / gsettings_total
            if gsettings_total
            else 100
        )
        if gsettings_percent > COMPLETENESS_THRESHOLD:
            gsettings_over_threshold_count += 1

        if not args.verbose:
            ui = group_summary(
                grouped_entries["Camorama UI"],
                group_counts["Camorama UI"],
                obsolete_counts["Camorama UI"],
            )
            gsettings = group_summary(
                grouped_entries["GSettings"],
                group_counts["GSettings"],
                obsolete_counts["GSettings"],
            )
            translated_total = sum(counts["TRANSLATED"] for counts in group_counts.values())
            done_percent = 100 * translated_total / len(source_entries) if source_entries else 100
            language = f"{locale} ({done_percent:.0f}% done)"
            summary_rows.append((language, ui, gsettings))
            continue

        print(f"\n[{locale}]")
        print(
            "  Camorama UI: "
            + group_summary(
                grouped_entries["Camorama UI"],
                group_counts["Camorama UI"],
                obsolete_counts["Camorama UI"],
            )
        )
        print(
            "  GSettings: "
            + group_summary(
                grouped_entries["GSettings"],
                group_counts["GSettings"],
                obsolete_counts["GSettings"],
            )
        )
        for key, source in source_by_key.items():
            translated = active_by_key.get(key)
            status = entry_status(translated)
            shown_translation = translation_text(translated) if translated else ""
            print(
                f"  [{message_group(source)}] {status:13} "
                f"{json.dumps(source.msgid, ensure_ascii=False)}"
                + (f" -> {json.dumps(shown_translation, ensure_ascii=False)}" if shown_translation else "")
            )
        if obsolete:
            print("  Obsolete entries (not in current template):")
            for entry in obsolete:
                print(
                    f"    {json.dumps(entry.msgid, ensure_ascii=False)}"
                    + (f" -> {json.dumps(translation_text(entry), ensure_ascii=False)}" if translation_text(entry) else " (empty)")
                )
    if not args.verbose and not args.summary:
        print(format_table(("Language", "UI", "Gsettings"), summary_rows))
    language_count = len(locales)
    ui_over_threshold_percent = 100 * ui_over_threshold_count / language_count if language_count else 0
    gsettings_over_threshold_percent = (
        100 * gsettings_over_threshold_count / language_count if language_count else 0
    )
    print(
        f"Languages: {language_count}; UI >{COMPLETENESS_THRESHOLD}%: {ui_over_threshold_count} "
        f"({ui_over_threshold_percent:.0f}%); GSettings >{COMPLETENESS_THRESHOLD}%: {gsettings_over_threshold_count} "
        f"({gsettings_over_threshold_percent:.0f}%)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
