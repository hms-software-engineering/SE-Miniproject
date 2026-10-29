#!/usr/bin/env python3
"""Measure concurrent appointment-booking latency against a prepared HMS API."""

import argparse
import concurrent.futures
import json
import math
import ssl
import sys
import threading
import time
import urllib.error
import urllib.request


USER_COUNT = 100
EXPECTED_STATUS = 201


def load_scenario(path):
    with open(path, encoding="utf-8") as scenario_file:
        scenario = json.load(scenario_file)
    requests = scenario.get("requests") if isinstance(scenario, dict) else None
    if not isinstance(requests, list) or len(requests) != USER_COUNT:
        raise ValueError(
            f"Scenario must contain exactly {USER_COUNT} request objects."
        )

    required = {"patientId", "doctorId", "date", "time", "token"}
    for index, request in enumerate(requests):
        if not isinstance(request, dict) or set(request) != required:
            raise ValueError(
                f"Request {index + 1} must contain exactly: "
                + ", ".join(sorted(required))
            )
        if any(not isinstance(request[key], str) or not request[key]
               for key in required):
            raise ValueError(f"Request {index + 1} fields must be non-empty strings.")
    return requests


def percentile(values, percentile_value):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(percentile_value * len(ordered)) - 1)]


def send_booking(base_url, request_data, tls_context, timeout, start_barrier):
    start_barrier.wait()
    body = json.dumps({
        key: request_data[key]
        for key in ("patientId", "doctorId", "date", "time")
    }).encode("utf-8")
    request = urllib.request.Request(
        base_url.rstrip("/") + "/api/v1/appointments",
        data=body,
        headers={
            "Authorization": "Bearer " + request_data["token"],
            "Content-Type": "application/json",
        },
        method="POST",
    )
    started = time.perf_counter()
    try:
        with urllib.request.urlopen(
            request, timeout=timeout, context=tls_context
        ) as response:
            status = response.status
            response.read()
    except urllib.error.HTTPError as error:
        status = error.code
        error.close()
    except (urllib.error.URLError, TimeoutError, OSError) as error:
        reason = getattr(error, "reason", error)
        return (
            time.perf_counter() - started,
            None,
            f"{type(error).__name__}: {reason}",
        )
    elapsed = time.perf_counter() - started
    return elapsed, status, None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", required=True,
                        help="HTTPS server origin, e.g. https://localhost:8443")
    parser.add_argument("--scenario", required=True,
                        help="JSON file containing 100 distinct booking requests")
    parser.add_argument("--ca-file",
                        help="PEM CA certificate for a non-system-trusted server")
    parser.add_argument("--timeout", type=float, default=30.0,
                        help="Per-request timeout in seconds (default: 30)")
    parser.add_argument("--threshold-ms", type=float, default=3000.0,
                        help="Required p95 latency in milliseconds (default: 3000)")
    args = parser.parse_args()

    try:
        requests = load_scenario(args.scenario)
        tls_context = ssl.create_default_context(cafile=args.ca_file)
    except (OSError, ValueError, json.JSONDecodeError, ssl.SSLError) as error:
        parser.error(str(error))

    start_barrier = threading.Barrier(USER_COUNT)
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=USER_COUNT
    ) as executor:
        futures = [
            executor.submit(
                send_booking, args.base_url, item, tls_context,
                args.timeout, start_barrier
            )
            for item in requests
        ]
        results = [future.result() for future in futures]

    latencies = [elapsed for elapsed, _, _ in results]
    successes = sum(status == EXPECTED_STATUS for _, status, _ in results)
    failures = USER_COUNT - successes
    p95_ms = percentile(latencies, 0.95) * 1000
    print(f"Concurrent users: {USER_COUNT}")
    print(f"Successful bookings: {successes}/{USER_COUNT}")
    print(f"Failed bookings: {failures}")
    print(f"p50 latency: {percentile(latencies, 0.50) * 1000:.1f} ms")
    print(f"p95 latency: {p95_ms:.1f} ms (target: < {args.threshold_ms:.1f} ms)")
    print(f"Maximum latency: {max(latencies) * 1000:.1f} ms")

    if failures:
        print("HTTP status / transport errors:", file=sys.stderr)
        for _, status, error in results:
            if status != EXPECTED_STATUS:
                print(f"  status={status}, error={error}", file=sys.stderr)
    return 0 if failures == 0 and p95_ms < args.threshold_ms else 1


if __name__ == "__main__":
    sys.exit(main())
