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
from urllib.parse import urlencode
from urllib.request import Request, urlopen


def main():
    binary = Path(sys.argv[1]).resolve(strict=True)
    expected_version = sys.argv[2] if len(sys.argv) > 2 else "5.2.4"
    saved_choking_algorithm = os.environ.get("SEEDBOX_TEST_CHOKING_ALGORITHM")
    if saved_choking_algorithm not in (None, "FixedSlots", "RateBased"):
        raise ValueError("SEEDBOX_TEST_CHOKING_ALGORITHM must be FixedSlots or RateBased")
    expected_slots = {"FixedSlots": 0, "RateBased": 1}.get(
        saved_choking_algorithm, 1 if expected_version == "5.2.4" else 0)
    scenario = f"saved {saved_choking_algorithm}" if saved_choking_algorithm else "fresh profile"
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
            "Session\\InterfaceAddress=127.0.0.1\nSession\\Port=0\n"
            + (f"Session\\ChokingAlgorithm={saved_choking_algorithm}\n"
               if saved_choking_algorithm else ""),
            encoding="utf-8",
        )

        def start():
            return subprocess.Popen(
                [str(binary), f"--profile={profile}", f"--webui-port={port}",
                 "--confirm-legal-notice"],
                env={**os.environ, "LANG": "C.UTF-8"},
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )

        def stop(proc):
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
                    raise RuntimeError("qB did not stop cleanly within 15 seconds")

        proc = start()
        try:
            def get(endpoint):
                with urlopen(f"http://127.0.0.1:{port}/api/v2/{endpoint}", timeout=2) as response:
                    return response.read().decode()

            def set_preferences(changes):
                data = urlencode({"json": json.dumps(changes)}).encode()
                request = Request(f"http://127.0.0.1:{port}/api/v2/app/setPreferences", data=data)
                with urlopen(request, timeout=3) as response:
                    assert response.status == 200

            def wait_for_webui():
                deadline = time.monotonic() + 30
                while True:
                    if proc.poll() is not None:
                        raise RuntimeError(f"qB exited before WebUI startup: {proc.returncode}")
                    try:
                        return get("app/version")
                    except (URLError, TimeoutError):
                        if time.monotonic() >= deadline:
                            raise RuntimeError("WebUI did not become ready within 30 seconds")
                        time.sleep(0.2)

            # Catches a wrong fresh-profile default or ignored saved/API-selected value.
            def check_upload_slots(preferences, slots, label):
                for key, value in {"upload_slots_behavior": slots,
                                   "upload_choking_algorithm": 1,
                                   "max_uploads": -1, "max_uploads_per_torrent": -1}.items():
                    assert preferences.get(key) == value, \
                        f"{label}: {key}: {preferences.get(key)!r} != {value!r}"

            version = wait_for_webui()
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
            check_upload_slots(prefs, expected_slots, scenario)
            assert json.loads(get("torrents/info")) == []
            expected_guard = os.environ.get("SEEDBOX_EXPECT_RESOURCE_GUARD")
            guard_metrics = None
            if expected_guard:
                logs = json.loads(get("log/main?normal=true&info=true&warning=true&critical=true"))
                marker = f"[resource-guard] mode={expected_guard}"
                assert any(marker in entry.get("message", "") for entry in logs), \
                    "Resource guard startup mode was not reported"
                if expected_guard == "off":
                    assert not any("[resource-guard] metrics=" in entry.get("message", "")
                                   for entry in logs), "Disabled guard reported pressure sampling"
                if expected_guard == "on":
                    initial = "[resource-guard] effective_high_kib=10240 baseline_high_kib=10240 low_kib=3072 level=0"
                    assert any(initial in entry.get("message", "") for entry in logs), \
                        "Initial LT watermark readback did not match the user baseline"
                    set_preferences({"send_buffer_watermark": 12288,
                                     "send_buffer_low_watermark": 2048})
                    deadline = time.monotonic() + 10
                    changed = "[resource-guard] effective_high_kib=12288 baseline_high_kib=12288 low_kib=2048 level=0"
                    while True:
                        logs = json.loads(get("log/main?normal=true&info=true&warning=true&critical=true"))
                        if any(changed in entry.get("message", "") for entry in logs):
                            break
                        assert time.monotonic() < deadline, "Updated user baseline was not read back from LT"
                        time.sleep(0.1)
                    latest = json.loads(get("app/preferences"))
                    assert latest["send_buffer_watermark"] == 12288
                    assert latest["send_buffer_low_watermark"] == 2048
                    deadline = time.monotonic() + 15
                    while True:
                        logs = json.loads(get("log/main?normal=true&info=true&warning=true&critical=true"))
                        for entry in logs:
                            message = entry.get("message", "")
                            if "[resource-guard] metrics=ready" in message:
                                guard_metrics = "ready"
                            elif "[resource-guard] metrics=unavailable" in message:
                                guard_metrics = "unavailable"
                        if guard_metrics:
                            break
                        assert time.monotonic() < deadline, "Enabled guard did not report metric validity"
                        time.sleep(0.1)
            for slots in (0, 1):
                set_preferences({"upload_slots_behavior": slots})
                check_upload_slots(json.loads(get("app/preferences")), slots, "API readback")
                stop(proc)
                assert proc.returncode == 0, f"qB did not exit cleanly: {proc.returncode}"
                proc = start()
                assert wait_for_webui() == version
                check_upload_slots(json.loads(get("app/preferences")), slots, "clean restart")
                assert json.loads(get("torrents/info")) == []
            print(json.dumps({"qBittorrent": version, "build": build,
                              "custom_defaults_checked": len(expected),
                              "upload_slots_scenario_checked": scenario,
                              "upload_slots_restart_values_checked": [0, 1],
                              "resource_guard_checked": expected_guard,
                              "resource_guard_metrics": guard_metrics}, sort_keys=True))
        finally:
            stop(proc)


if __name__ == "__main__":
    main()
