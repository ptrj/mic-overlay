#!/usr/bin/env python3
"""Stable fake audio, real KWin: measure idle work and protocol input settings."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

binary = Path(os.environ.get("MIC_OVERLAY_BINARY", "/build/acceptance-clean/mic-overlay"))
results = Path("/build/acceptance-idle")
results.mkdir(exist_ok=True)


def snapshot(pid):
    # Fields after the parenthesized comm begin at field 3 (state).
    fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
    ticks = int(fields[11]) + int(fields[12])
    status = Path(f"/proc/{pid}/status").read_text()
    rss = int(re.search(r"^VmRSS:\s+(\d+)", status, re.M)[1])
    return ticks, rss


with tempfile.TemporaryDirectory(prefix="mic-overlay-idle-") as directory:
    fixture = Path(directory)
    for name in ("wpctl", "pw-dump"):
        shutil.copyfile(f"/source/tests/fake-{name}.sh", fixture / name)
        (fixture / name).chmod(0o700)
    (fixture / "state").write_text("muted")
    (fixture / "monitor-mode").write_text("valid")
    event = [{"id": 123, "type": "PipeWire:Interface:Node", "info": {
        "props": {"media.class": "Audio/Source", "device.id": 44}}}]
    (fixture / "events").write_text(json.dumps(event) + "\n")
    env = dict(os.environ, PATH=f"{fixture}:{os.environ['PATH']}", WAYLAND_DEBUG="1",
               QT_FORCE_STDERR_LOGGING="1", QT_QUICK_BACKEND="software")
    log_path = results / "wayland.log"
    with log_path.open("wb") as log_file:
        process = subprocess.Popen([str(binary), "--no-tray"], env=env, stdout=log_file, stderr=log_file)
        try:
            for _ in range(100):
                assert process.poll() is None, "Overlay exited during startup"
                if "PipeWire event monitoring active" in log_path.read_text():
                    break
                time.sleep(0.05)
            else:
                raise AssertionError("Event monitoring did not become active")
            time.sleep(2)  # Exclude startup queries and the first rendered frames.
            first_log = log_path.read_text()
            first_queries = (fixture / "queries").read_text().count("query\n")
            first_ticks, _ = snapshot(process.pid)
            started = time.monotonic()
            time.sleep(15)
            elapsed = time.monotonic() - started
            last_ticks, rss = snapshot(process.pid)
            last_log = log_path.read_text()
            last_queries = (fixture / "queries").read_text().count("query\n")
            attachments = lambda text: len(re.findall(r"wl_surface#\d+\.attach\(wl_buffer", text))
            commits = lambda text: len(re.findall(r"wl_surface#\d+\.commit\(", text))
            assert last_queries == first_queries, "Idle wpctl queries detected"
            assert attachments(last_log) == attachments(first_log), "Idle buffer rendering detected"
            assert commits(last_log) == commits(first_log), "Idle surface commits detected"
            report = {
                "seconds": round(elapsed, 3),
                "overlay_cpu_percent_of_one_core": round(
                    (last_ticks - first_ticks) / os.sysconf("SC_CLK_TCK") / elapsed * 100, 4),
                "overlay_rss_kib": rss,
                "wpctl_queries_during_idle": last_queries - first_queries,
                "buffer_attachments_during_idle": attachments(last_log) - attachments(first_log),
                "surface_commits_during_idle": commits(last_log) - commits(first_log),
                "startup_queries": first_queries,
            }
            # Show -> hide -> show with a fake source error; host audio is unchanged.
            for state, marker in (("unmuted", "state changed: unmuted"),
                                  ("error", "wpctl query failed"),
                                  ("muted", "state changed: muted")):
                offset = log_path.stat().st_size
                (fixture / "state").write_text(state)
                with (fixture / "events").open("a") as stream:
                    stream.write(json.dumps([{
                        "id": 44, "info": {"params": {"Route": []}}}]) + "\n")
                for _ in range(80):
                    assert process.poll() is None, "Overlay exited during state change"
                    if marker in log_path.read_text()[offset:]:
                        break
                    time.sleep(0.05)
                else:
                    raise AssertionError(f"State change not observed: {state}")
                time.sleep(0.2)
            trace = log_path.read_text()
            layers = re.findall(r"get_layer_surface\([^\n]+, 3, \"mic-overlay\"\)", trace)
            assert layers, "LayerOverlay surface not found"
            assert ".set_keyboard_interactivity(0)" in trace
            assert ".set_exclusive_zone(-1)" in trace
            regions = re.findall(r"set_input_region\(wl_region#(\d+)\)", trace)
            assert regions, "No explicit click-through input region"
            for region in regions:
                assert not re.search(rf"wl_region#{region}\.add\(", trace), "Nonempty input region"
            assert not re.search(r"wl_(keyboard|pointer)#\d+\.enter\(", trace), "Overlay received input focus"
            assert not re.search(r"get_xdg_surface\(", trace), "Overlay created a normal application surface"
            report["state_changes_and_input_protocol"] = "passed"
            (results / "report.json").write_text(json.dumps(report, indent=2) + "\n")
            print(json.dumps(report, indent=2), flush=True)
        finally:
            process.terminate()
            try:
                code = process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise AssertionError("Overlay did not terminate cleanly")
        assert code == 0, f"Overlay exit status {code}"
print(f"PASS: idle and input protocol; results: {results}")
