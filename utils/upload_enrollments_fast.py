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


def post_enrollment(session, url, student_id, course_id, timeout=10):
    try:
        body = {'student_id': student_id, 'course_id': course_id}
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
    parser.add_argument('--csv', default='example_enrollments.csv')
    parser.add_argument('--api', default='http://localhost:8080')
    parser.add_argument('--workers', type=int, default=8, help='number of concurrent workers')
    parser.add_argument('--timeout', type=int, default=10, help='per-request timeout seconds')
    parser.add_argument('--retries', type=int, default=3, help='per-request retries')
    args = parser.parse_args()

    url = args.api.rstrip('/') + '/enrollment'
    rows = load_rows(args.csv)
    total = len(rows)
    print(f'Loaded {total} rows from {args.csv}; uploading to {url} with {args.workers} workers')

    session = make_session(retries=args.retries, pool_maxsize=args.workers)

    done = 0
    done_lock = threading.Lock()

    def worker(row):
        sid = row.get('student_id') or row.get('student') or row.get('s_id') or ''
        cid = row.get('course_id') or row.get('course') or row.get('c_id') or ''
        status, resp = post_enrollment(session, url, sid, cid, timeout=args.timeout)
        nonlocal done
        with done_lock:
            done += 1
            current = done
        return (sid, cid, status, resp, current)

    successes = 0
    failures = 0
    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        futures = [ex.submit(worker, row) for row in rows]
        for fut in as_completed(futures):
            sid, cid, status, resp, current = fut.result()
            if status is None:
                failures += 1
                print(f'[{current}/{total}] {sid}->{cid} FAILED: {resp}')
            elif 200 <= status < 300:
                successes += 1
                # Minimal logging to reduce overhead
                if current % 100 == 0 or current <= 10:
                    print(f'[{current}/{total}] {sid}->{cid} OK ({status})')
            else:
                failures += 1
                print(f'[{current}/{total}] {sid}->{cid} ERROR ({status}): {resp}')

    print(f'Done. {successes}/{total} succeeded, {failures} failed.')


if __name__ == '__main__':
    main()
