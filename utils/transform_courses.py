import json
import csv
import random
import argparse
import re

parser = argparse.ArgumentParser()
parser.add_argument('--maj', default='major.json')
parser.add_argument('--out', default='example_courses.csv')
args = parser.parse_args()

def clean_id(s):
    return re.sub(r'[^A-Za-z0-9]', '', str(s))

with open(args.maj, 'r', encoding='utf-8') as f:
    majors = json.load(f)

rows = []
suffixes = ['导论','概论','基础','实践']
types = ['required','elective','general']
semesters = ['fall','spring','summer']

idx = 1
for m in majors:
    code = m.get('专业代码') or ''
    name = m.get('专业名称') or '课程'

    course_id = f"C{str(idx).zfill(4)}"

    cname = name + random.choice(suffixes)
    ctype = random.choice(types)
    credit = random.choice([2,3,4,5])
    total_hours = credit * 16
    # choose lab hours between 0 and up to 30% of total (rounded to nearest 2)
    max_lab = int(total_hours * 0.3)
    lab_hours = random.randint(0, max_lab) if max_lab>0 else 0
    # ensure lecture + lab == total
    lecture_hours = total_hours - lab_hours
    semester = random.choice(semesters)

    rows.append({
        'course_id': course_id,
        'name': cname,
        'type': ctype,
        'credit': credit,
        'total_hours': total_hours,
        'lecture_hours': lecture_hours,
        'lab_hours': lab_hours,
        'semester': semester
    })
    idx += 1

with open(args.out, 'w', newline='', encoding='utf-8') as csvfile:
    fieldnames = ['course_id','name','type','credit','total_hours','lecture_hours','lab_hours','semester']
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()
    for r in rows:
        writer.writerow(r)

print(f'Wrote {len(rows)} courses to {args.out}')
