import csv
import json
import argparse
import urllib.request
import urllib.error

parser = argparse.ArgumentParser()
parser.add_argument('--csv', default='example_enrollments.csv')
parser.add_argument('--api', default='http://localhost:8080')
args = parser.parse_args()

api_base = args.api.rstrip('/')
url = api_base + '/enrollment'

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
    sid = row.get('student_id') or row.get('student') or row.get('s_id') or ''
    cid = row.get('course_id') or row.get('course') or row.get('c_id') or ''

    body = {
        'student_id': sid,
        'course_id': cid
    }

    print(f'[{i}/{total}] Adding enrollment {sid} -> {cid}...', end=' ')
    status, resp = post_json(url, body)
    if status is None:
        print(f'FAILED: connection error: {resp}')
    elif 200 <= status < 300:
        print(f'OK ({status})')
        done += 1
    else:
        print(f'ERROR ({status}): {resp}')

print(f'Done. {done}/{total} succeeded.')
