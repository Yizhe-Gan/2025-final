import csv
import argparse
import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading


def make_session(retries=3, backoff_factor=0.5, pool_connections=100, pool_maxsize=100):
    s = requests.Session()
    retry = Retry(total=retries, backoff_factor=backoff_factor, status_forcelist=(500,502,503,504))
    adapter = HTTPAdapter(pool_connections=pool_connections, pool_maxsize=pool_maxsize, max_retries=retry)
    s.mount('http://', adapter)
    s.mount('https://', adapter)
    return s


def post_course(session, url, row, timeout=10):
    # Extract and coerce fields similar to upload_courses.py
    cid = row.get('course_id') or row.get('id') or ''
    name = row.get('name') or ''
    ctype = row.get('type') or ''
    credit = row.get('credit') or row.get('credits') or '0'
    total_hours = row.get('total_hours') or ''
    lecture_hours = row.get('lecture_hours') or ''
    lab_hours = row.get('lab_hours') or ''
    semester = row.get('semester') or ''

    try:
        credit_val = float(credit)
    except Exception:
        credit_val = 0
    try:
        total_hours_val = float(total_hours)
    except Exception:
        total_hours_val = 0
    try:
        lecture_hours_val = float(lecture_hours)
    except Exception:
        lecture_hours_val = 0
    try:
        lab_hours_val = float(lab_hours)
    except Exception:
        lab_hours_val = 0

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

    try:
        resp = session.post(url, json=body, timeout=timeout)
        return resp.status_code, resp.text
    except Exception as e:
        return None, str(e)


def load_rows(path):
    with open(path, newline='', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        return [r for r in reader]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', default='example_courses.csv')
    parser.add_argument('--api', default='http://localhost:8080')
    parser.add_argument('--workers', type=int, default=8, help='number of concurrent workers')
    parser.add_argument('--timeout', type=int, default=10, help='per-request timeout seconds')
    parser.add_argument('--retries', type=int, default=3, help='per-request retries')
    args = parser.parse_args()

    url = args.api.rstrip('/') + '/course/add'
    rows = load_rows(args.csv)
    total = len(rows)
    print(f'Loaded {total} rows from {args.csv}; uploading to {url} with {args.workers} workers')

    session = make_session(retries=args.retries, pool_maxsize=args.workers)

    done = 0
    done_lock = threading.Lock()

    def worker(row):
        cid = row.get('course_id') or row.get('id') or ''
        status, resp = post_course(session, url, row, timeout=args.timeout)
        nonlocal done
        with done_lock:
            done += 1
            current = done
        return (cid, status, resp, current)

    successes = 0
    failures = 0
    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        futures = [ex.submit(worker, row) for row in rows]
        for fut in as_completed(futures):
            cid, status, resp, current = fut.result()
            if status is None:
                failures += 1
                print(f'[{current}/{total}] {cid} FAILED: {resp}')
            elif 200 <= status < 300:
                successes += 1
                if current % 100 == 0 or current <= 10:
                    print(f'[{current}/{total}] {cid} OK ({status})')
            else:
                failures += 1
                print(f'[{current}/{total}] {cid} ERROR ({status}): {resp}')

    print(f'Done. {successes}/{total} succeeded, {failures} failed.')


if __name__ == '__main__':
    main()
