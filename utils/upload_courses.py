import csv
import json
import argparse
import urllib.request
import urllib.error

parser = argparse.ArgumentParser()
parser.add_argument('--csv', default='example_courses.csv')
parser.add_argument('--api', default='http://localhost:8080')
args = parser.parse_args()

api_base = args.api.rstrip('/')
url = api_base + '/course/add'

def post_json(url, obj):
    data = json.dumps(obj).encode('utf-8')
    req = urllib.request.Request(url, data=data, headers={'Content-Type':'application/json'}, method='POST')
    try:
        with urllib.request.urlopen(req) as resp:
            body = resp.read().decode('utf-8')
            return resp.getcode(), body
    except urllib.error.HTTPError as e:
        try:
            body = e.read().decode('utf-8')
        except Exception:
            body = ''
        return e.code, body
    except urllib.error.URLError as e:
        return None, str(e)

fpath = args.csv
with open(fpath, newline='', encoding='utf-8-sig') as f:
    reader = csv.DictReader(f)
    rows = list(reader)

total = len(rows)
print(f'Found {total} rows in {fpath}')

done = 0
for i, row in enumerate(rows, start=1):
    cid = row.get('course_id') or row.get('id') or ''
    name = row.get('name') or ''
    ctype = row.get('type') or ''
    credit = row.get('credit') or row.get('credits') or '0'
    total_hours = row.get('total_hours') or ''
    lecture_hours = row.get('lecture_hours') or ''
    lab_hours = row.get('lab_hours') or ''
    semester = row.get('semester') or ''
    
    try: credit_val = float(credit)
    except Exception: credit_val = 0
    
    try: total_hours_val = float(total_hours)
    except Exception: total_hours_val = 0
    
    try: lecture_hours_val = float(lecture_hours)
    except Exception: lecture_hours_val = 0
    
    try: lab_hours_val = float(lab_hours)
    except Exception: lab_hours_val = 0

    body = {
        'course_id': cid,
        'credit': credit_val,
        'name': name,
        'type': ctype,
        'total_hours': total_hours_val,
        'lecture_hours': lecture_hours_val,
        'lab_hours': lab_hours_val,
        'semester': semester
    }

    print(f'[{i}/{total}] Adding course {cid}...', end=' ')
    status, resp = post_json(url, body)
    if status is None:
        print(f'FAILED: connection error: {resp}')
    elif 200 <= status < 300:
        print(f'OK ({status})')
        done += 1
    else:
        print(f'ERROR ({status}): {resp}')

print(f'Done. {done}/{total} succeeded.')

