"""
generate_enrollments.py

Read `example_students.csv` and `example_courses.csv`, assign each student to a
random number of courses, and write `example_enrollments.csv`.

Usage:
  python generate_enrollments.py --students example_students.csv \
      --courses example_courses.csv --out example_enrollments.csv --min 1 --max 6 --seed 42

The script avoids duplicate enrollments per student and is deterministic with a seed.
"""
import csv
import argparse
import random
import sys
from typing import List


def read_ids_from_csv(path: str, key: str) -> List[str]:
    ids = []
    with open(path, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        if key not in reader.fieldnames:
            raise ValueError(f"CSV {path} missing expected column: {key}")
        for row in reader:
            val = row.get(key)
            if val is None:
                continue
            ids.append(str(val))
    return ids


def write_enrollments(path: str, enrollments: List[tuple]):
    with open(path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['student_id', 'course_id'])
        for s, c in enrollments:
            writer.writerow([s, c])


def generate_enrollments(student_ids: List[str], course_ids: List[str], min_per: int, max_per: int) -> List[tuple]:
    enrollments = []
    if not course_ids:
        return enrollments

    for sid in student_ids:
        max_allowed = min(max_per, len(course_ids))
        num = random.randint(min_per, max_allowed) if max_allowed >= min_per else max_allowed
        # pick unique courses
        chosen = random.sample(course_ids, num) if num > 0 else []
        for c in chosen:
            enrollments.append((sid, c))

    return enrollments


def main(argv=None):
    parser = argparse.ArgumentParser(description="Generate example_enrollments.csv from students and courses CSVs")
    parser.add_argument('--students', default='example_students.csv', help='path to students CSV')
    parser.add_argument('--courses', default='example_courses.csv', help='path to courses CSV')
    parser.add_argument('--out', default='example_enrollments.csv', help='output CSV path')
    parser.add_argument('--min', type=int, default=2, help='minimum courses per student')
    parser.add_argument('--max', type=int, default=13, help='maximum courses per student')
    parser.add_argument('--seed', type=int, default=None, help='random seed for reproducibility')

    args = parser.parse_args(argv)

    if args.min < 0 or args.max < 0:
        print('min and max must be non-negative', file=sys.stderr)
        sys.exit(2)
    if args.min > args.max:
        print('min cannot be greater than max', file=sys.stderr)
        sys.exit(2)

    if args.seed is not None:
        random.seed(args.seed)

    try:
        students = read_ids_from_csv(args.students, 'student_id')
    except Exception as e:
        print(f'Failed to read students CSV: {e}', file=sys.stderr)
        sys.exit(1)

    try:
        courses = read_ids_from_csv(args.courses, 'course_id')
    except Exception as e:
        print(f'Failed to read courses CSV: {e}', file=sys.stderr)
        sys.exit(1)

    enrollments = generate_enrollments(students, courses, args.min, args.max)
    write_enrollments(args.out, enrollments)

    print(f'Wrote {len(enrollments)} enrollments to {args.out}')


if __name__ == '__main__':
    main()
