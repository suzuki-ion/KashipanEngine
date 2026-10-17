"""Protocol, native transport integration and Tk widget smoke regression."""
import argparse
import copy
import json
from pathlib import Path
import queue
import socket
import subprocess
import tempfile
import time
import unittest

from protocol import (MAX_PACKET, PacketDecoder, Receiver, load_capture,
                      save_capture, validate_snapshot)


def sample(frame=1):
    return {"type": "kashipan.cpu", "version": 1, "pid": 12, "frameId": frame,
            "frameMs": 16.7, "fps": 59.9, "frames": 2, "sampleCount": 60,
            "results": [{"name": "Update.日本語", "milliseconds": 3.0,
                         "averageMs": 2.0, "peakMs": 3.0, "calls": 2}],
            "receivedAt": 1700000000 + frame / 10, "session": "test"}


class ProtocolTests(unittest.TestCase):
    def test_fragmented_and_coalesced_packets(self):
        value = sample()
        raw = (json.dumps(value, ensure_ascii=False) + "\n").encode()
        decoder = PacketDecoder()
        self.assertEqual(decoder.feed(raw[:7]), [])
        self.assertEqual(decoder.feed(raw[7:] + raw), [value, value])

    def test_reject_invalid_or_unbounded_packets(self):
        decoder = PacketDecoder()
        with self.assertRaises(ValueError):
            decoder.feed(b"x" * (MAX_PACKET + 1))
        for key, value in (("fps", float("nan")), ("frameMs", -1), ("version", 2),
                           ("results", [{}]), ("sampleCount", 601), ("frameId", True),
                           ("frameMs", 10**1000)):
            packet = sample()
            packet[key] = value
            with self.assertRaises(ValueError):
                validate_snapshot(packet)

    def test_capture_round_trip_and_rejection(self):
        output = Path(__file__).resolve().parents[3] / "Generated" / "ProfilerTests"
        output.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=output) as directory:
            path = Path(directory) / "記録.jsonl"
            values = [sample(1), sample(2)]
            save_capture(path, values)
            self.assertEqual(load_capture(path), values)
            old = path.read_bytes()
            invalid = copy.deepcopy(values)
            invalid[1]["fps"] = float("nan")
            with self.assertRaises(ValueError):
                save_capture(path, invalid)
            self.assertEqual(path.read_bytes(), old)  # Failed save preserves existing capture.
            path.write_text('{"format":"other"}\n', encoding="utf-8")
            with self.assertRaises(ValueError):
                load_capture(path)


def receive_sample(receiver, timeout=5):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        try:
            kind, value = receiver.events.get(timeout=0.2)
            if kind == "sample":
                return value
        except queue.Empty:
            pass
    raise AssertionError("CPU snapshot did not arrive")


def integration(harness, with_gui=False):
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    process = subprocess.Popen([harness, str(port), "8"], stdout=subprocess.PIPE, text=True)
    receiver = None
    try:
        status = process.stdout.readline().strip()
        assert "127.0.0.1:" in status, status
        receiver = Receiver(port)
        receiver.start()
        first = receive_sample(receiver)
        second = receive_sample(receiver)
        assert second["frameId"] > first["frameId"]
        assert {row["name"] for row in first["results"]} == {"Test.Frame", "Test.日本語"}
        assert first["frameMs"] > 0 and first["fps"] > 0
        receiver.stop()
        receiver = Receiver(port)
        receiver.start()
        reconnect = receive_sample(receiver)
        assert reconnect["session"] != first["session"]
        receiver.stop()
        receiver = None
        if with_gui:
            import tkinter as tk
            from profiler import ProfilerApp
            root = tk.Tk()
            root.withdraw()
            app = ProfilerApp(root, port)
            try:
                until = time.monotonic() + 3
                while time.monotonic() < until and len(app.samples) < 2:
                    root.update()
                    time.sleep(0.02)
                assert len(app.samples) >= 2, app.connection_status
                app.toggle_pause()
                count = len(app.samples)
                until = time.monotonic() + 0.3
                while time.monotonic() < until:
                    root.update()
                    time.sleep(0.02)
                assert len(app.samples) == count
                previous_frame = app.samples[-1]["frameId"]
                app.toggle_pause()
                until = time.monotonic() + 1
                while time.monotonic() < until and app.samples[-1]["frameId"] == previous_frame:
                    root.update()
                    time.sleep(0.02)
                assert app.samples[-1]["frameId"] > previous_frame
                print("Live Tk integration passed: native samples displayed, pause and resume")
            finally:
                app.close()
        # A connected viewer that stops reading must not stop the simulated game.
        with socket.create_connection(("127.0.0.1", port), timeout=2):
            output, _ = process.communicate(timeout=10)
        frames = int(output.split("Completed ")[-1].strip())
        assert frames > 100, frames
        print(f"Native TCP integration passed: reconnect, Unicode, unread client; {frames} frames")
    finally:
        if receiver:
            receiver.stop()
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=3)
        process.stdout.close()


def gui_smoke():
    import tkinter as tk
    from collections import deque
    from profiler import ProfilerApp
    root = tk.Tk()
    root.withdraw()
    failures = []
    root.report_callback_exception = lambda *args: failures.append(args)
    app = ProfilerApp(root, connect=False)
    try:
        app.samples = deque([sample(index) for index in range(1, 20)], maxlen=600)
        app.selection = 18
        app.render()
        root.update()
        assert len(app.table.get_children()) == 1
        app.scrub("4")
        assert app.paused and app.current()["frameId"] == 5
        app.table.selection_set("0")
        root.update()
        assert app.selected_process == "Update.日本語"
        app.filter.set("missing")
        root.update()
        assert not app.table.get_children()
        app.filter.set("")
        app.toggle_pause()
        assert app.selection == 18 and not app.paused
        app.sort("name")
        app.clear()
        root.update()
        assert not app.table.get_children() and not failures, failures
        print("Tk widget smoke passed: render, history, filter, selection, resume, clear")
    finally:
        app.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--harness")
    parser.add_argument("--gui", action="store_true")
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ProtocolTests)
    if not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful():
        raise SystemExit(1)
    if args.harness:
        integration(args.harness, args.gui)
    if args.gui:
        gui_smoke()
