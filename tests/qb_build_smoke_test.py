#!/usr/bin/env python3
"""Verify a custom Linux nox build using an isolated, localhost-only profile."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
from urllib.error import URLError
from urllib.request import urlopen


def main():
    binary = Path(sys.argv[1]).resolve(strict=True)
    expected_version = sys.argv[2] if len(sys.argv) > 2 else "5.2.4"
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        port = listener.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="seedbox-qb-smoke-") as profile:
        config = Path(profile) / "qBittorrent/config/qBittorrent.conf"
        config.parent.mkdir(parents=True)
        config.write_text(
            "[Preferences]\nWebUI\\Address=127.0.0.1\n"
            "WebUI\\LocalHostAuth=false\nConnection\\UPnP=false\n"
            "[BitTorrent]\nSession\\Interface=lo\n"
            "Session\\InterfaceAddress=127.0.0.1\nSession\\Port=0\n",
            encoding="utf-8",
        )
        proc = subprocess.Popen(
            [str(binary), f"--profile={profile}", f"--webui-port={port}",
             "--confirm-legal-notice"],
            env={**os.environ, "LANG": "C.UTF-8"},
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        try:
            def get(endpoint):
                with urlopen(f"http://127.0.0.1:{port}/api/v2/{endpoint}", timeout=2) as response:
                    return response.read().decode()

            deadline = time.monotonic() + 30
            while True:
                if proc.poll() is not None:
                    raise RuntimeError(f"qB exited before WebUI startup: {proc.returncode}")
                try:
                    version = get("app/version")
                    break
                except (URLError, TimeoutError):
                    if time.monotonic() >= deadline:
                        raise RuntimeError("WebUI did not become ready within 30 seconds")
                    time.sleep(0.2)
            assert version == f"v{expected_version}", version
            build = json.loads(get("app/buildInfo"))
            assert build["libtorrent"].startswith("1.2.20"), build
            prefs = json.loads(get("app/preferences"))
            expected = {
                "dht": False, "pex": False, "lsd": False,
                "async_io_threads": 4, "hashing_threads": 2,
                "file_pool_size": 5000, "checking_memory_use": 128,
                "disk_cache_ttl": 300, "disk_queue_size": 10485760,
                "enable_upload_suggestions": True,
                "send_buffer_watermark": 10240, "send_buffer_low_watermark": 3072,
                "send_buffer_watermark_factor": 200, "connection_speed": 500,
                "socket_backlog_size": 30000, "max_concurrent_http_announces": 75,
                "web_ui_csrf_protection_enabled": False,
            }
            for key, value in expected.items():
                assert prefs.get(key) == value, f"{key}: {prefs.get(key)!r} != {value!r}"
            assert json.loads(get("torrents/info")) == []
            print(json.dumps({"qBittorrent": version, "build": build,
                              "custom_defaults_checked": len(expected)}, sort_keys=True))
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()


if __name__ == "__main__":
    main()
