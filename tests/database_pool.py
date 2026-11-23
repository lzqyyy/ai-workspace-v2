from __future__ import annotations

import json
import os
import sys
import urllib.request
import uuid
from concurrent.futures import ThreadPoolExecutor

BASE = os.environ.get("BASE_URL", "http://127.0.0.1:8080")
REQUESTS = 256
WORKERS = 32


def health(_: int) -> bool:
    request = urllib.request.Request(f"{BASE}/api/v1/health")
    request.add_header("X-Request-ID", f"pool-{uuid.uuid4()}")
    with urllib.request.urlopen(request, timeout=20) as response:
        payload = json.loads(response.read().decode())
        assert response.status == 200, (response.status, payload)
        assert payload.get("status") == "ok", payload
        assert payload.get("database") == "ok", payload
        return True


def main() -> int:
    with ThreadPoolExecutor(max_workers=WORKERS) as pool:
        results = list(pool.map(health, range(REQUESTS)))
    assert all(results)
    print(json.dumps({"status": "PASS", "requests": REQUESTS, "workers": WORKERS}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
