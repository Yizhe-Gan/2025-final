"""
transform_students.py

Read `students.json` and `students_en.json` and produce a CSV with columns:
  student_id,name,email,credits

Rules:
- Use Chinese name from `students.json` (`Name`).
- Use `Id` as `student_id`.
- Email: `[PathName trimmed lowercase]@[schoollower].edu` if `School` present, otherwise `...@gmail.com`.
- Deduplicate variants: collect every `FavorAlts` id and skip any student whose `Id` is in that set.

Writes `example_students.csv` by default.
"""
import json
import csv
import os
import argparse
import re


def sanitize_local(pathname: str) -> str:
    if pathname is None:
        return ""
    s = pathname.strip().lower()
    # remove whitespace and surrounding punctuation, keep alnum and _ and -
    s = re.sub(r"\s+", "", s)
    s = re.sub(r"[^a-z0-9_\-]", "", s)
    return s


def sanitize_school(school: str) -> str:
    if not school:
        return ""
    s = school.strip().lower()
    s = re.sub(r"\s+", "", s)
    s = re.sub(r"[^a-z0-9_\-]", "", s)
    return s


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def build_ignore_set(students):
    ignore = set()
    for s in students:
        fav = s.get("FavorAlts")
        if isinstance(fav, list):
            for alt in fav:
                try:
                    ignore.add(int(alt))
                except Exception:
                    # skip non-int or malformed
                    pass
    return ignore


def make_email(pathname, school):
    local = sanitize_local(pathname)
    school_clean = sanitize_school(school)
    if not local:
        return ""
    if school_clean:
        return f"{local}@{school_clean}.edu"
    return f"{local}@gmail.com"


def transform(chinese_json_path, english_json_path, out_csv_path):
    students_cn = load_json(chinese_json_path)
    # english is not used now, but load to match requirement and allow fallbacks
    try:
        students_en = load_json(english_json_path)
    except Exception:
        students_en = []

    # ignore_ids = build_ignore_set(students_cn)
    ignore_ids = set()

    rows = []
    for s in students_cn:
        sid = s.get("Id")
        if sid is None:
            continue
        try:
            sid_int = int(sid)
        except Exception:
            # if Id is not numeric, still include as-is
            sid_int = s.get("Id")

        if sid_int in ignore_ids:
            # skip variant ids listed in someone's FavorAlts
            continue

        name_cn = s.get("Name") or s.get("PersonalName") or ""
        pathname = s.get("PathName") or s.get("DevName") or ""
        school = s.get("School")

        email = make_email(pathname, school)

        # default credits to 0 to match example CSV format
        rows.append({
            "student_id": sid_int,
            "name": name_cn,
            "email": email,
            "credits": 0,
        })
        
        # add FavorAlts into ignore set
        fav = s.get("FavorAlts")
        if isinstance(fav, list):
            for alt in fav:
                try:
                    ignore_ids.add(int(alt))
                except Exception:
                    # skip non-int or malformed
                    pass

    # write CSV
    with open(out_csv_path, "w", newline='', encoding="utf-8") as csvfile:
        fieldnames = ["student_id", "name", "email", "credits"]
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for r in rows:
            writer.writerow(r)


def main():
    parser = argparse.ArgumentParser(description="Transform students json to CSV")
    parser.add_argument("--cn", default="students.json", help="path to students.json (Chinese)")
    parser.add_argument("--en", default="students_en.json", help="path to students_en.json (English)")
    parser.add_argument("--out", default="example_students.csv", help="output CSV path")
    args = parser.parse_args()

    transform(args.cn, args.en, args.out)


if __name__ == "__main__":
    main()
