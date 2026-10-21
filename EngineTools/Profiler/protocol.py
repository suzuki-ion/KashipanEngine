"""CPU snapshot transport and capture format. Python standard library only."""
import json
import math
import os
import queue
import socket
import tempfile
import threading
import time
import uuid
from pathlib import Path

DEFAULT_PORT = 47180
MAX_PACKET = 1024 * 1024
MAX_SAMPLES = 600
MAX_CAPTURE = 64 * 1024 * 1024


def validate_snapshot(value):
    if not isinstance(value, dict) or value.get("type") != "kashipan.cpu" or value.get("version") != 1:
        raise ValueError("対応していない計測データです")
    for key in ("pid", "frameId", "frames", "sampleCount"):
        number = value.get(key)
        if type(number) is not int or not 0 <= number < 2**64:
            raise ValueError(f"不正な数値: {key}")
    if not 1 <= value["sampleCount"] <= 600 or value["frames"] > value["sampleCount"]:
        raise ValueError("不正な集計フレーム数です")
    def timing(number):
        return type(number) in (int, float) and 0 <= number <= 1e12 and math.isfinite(number)
    if not all(timing(value.get(key)) for key in ("frameMs", "fps")):
        raise ValueError("不正なフレーム時間です")
    rows = value.get("results")
    if not isinstance(rows, list) or len(rows) > 256:
        raise ValueError("不正な処理一覧です")
    names = set()
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError("不正な処理データです")
        name = row.get("name")
        if not isinstance(name, str) or len(name) > 4096 or name in names:
            raise ValueError("不正な処理名です")
        names.add(name)
        if not all(timing(row.get(key)) for key in ("milliseconds", "averageMs", "peakMs")):
            raise ValueError("不正な処理時間です")
        if type(row.get("calls")) is not int or not 0 <= row["calls"] < 2**64:
            raise ValueError("不正な呼び出し回数です")
    return value


class PacketDecoder:
    """TCP packets may contain partial lines or multiple complete lines."""
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data):
        self.buffer.extend(data)
        packets = []
        while (end := self.buffer.find(b"\n")) >= 0:
            if end > MAX_PACKET:
                raise ValueError("受信データが大きすぎます")
            line = bytes(self.buffer[:end])
            del self.buffer[:end + 1]
            packets.append(validate_snapshot(json.loads(line)))
        if len(self.buffer) > MAX_PACKET:
            raise ValueError("受信データが大きすぎます")
        return packets


class Receiver(threading.Thread):
    def __init__(self, port):
        super().__init__(daemon=True, name="CPU profile receiver")
        self.port = port
        self.events = queue.Queue(maxsize=64)
        self.stop_event = threading.Event()
        self.dropped = 0

    def publish(self, kind, value):
        try:
            self.events.put_nowait((kind, value))
        except queue.Full:
            try:
                self.events.get_nowait()
            except queue.Empty:
                pass
            self.dropped += 1
            self.events.put_nowait((kind, value))

    def run(self):
        while not self.stop_event.is_set():
            try:
                self.publish("status", "接続待ち / 127.0.0.1:" + str(self.port))
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.5) as connection:
                    connection.settimeout(0.3)
                    decoder = PacketDecoder()
                    session = uuid.uuid4().hex
                    self.publish("status", "接続中")
                    last_data = time.monotonic()
                    last_frame = None
                    while not self.stop_event.is_set():
                        try:
                            data = connection.recv(65536)
                        except socket.timeout:
                            if time.monotonic() - last_data > 3:
                                raise OSError("データの受信が止まりました")
                            continue
                        if not data:
                            raise OSError("エンジンとの接続が切れました")
                        last_data = time.monotonic()
                        for packet in decoder.feed(data):
                            key = (packet["pid"], packet["frameId"])
                            if key == last_frame or packet["frames"] == 0:
                                continue
                            last_frame = key
                            packet["receivedAt"] = time.time()
                            packet["session"] = session
                            self.publish("sample", packet)
            except (OSError, ValueError, UnicodeError, RecursionError) as error:
                self.publish("status", f"再接続待ち: {error}")
            self.stop_event.wait(1)

    def stop(self):
        self.stop_event.set()
        self.join(timeout=1)


def save_capture(path, samples):
    path = Path(path)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="wb", dir=path.parent, delete=False) as stream:
            temporary = stream.name
            stream.write(b'{"format":"kashipan.cpu.capture","version":1}\n')
            total = 0
            for sample in samples:
                validate_snapshot(sample)
                line = (json.dumps(sample, ensure_ascii=False, allow_nan=False) + "\n").encode("utf-8")
                if len(line) > MAX_PACKET or total + len(line) > MAX_CAPTURE - 128:
                    raise ValueError("保存データが上限を超えています")
                stream.write(line)
                total += len(line)
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary:
            os.unlink(temporary)


def load_capture(path):
    if Path(path).stat().st_size > MAX_CAPTURE:
        raise ValueError("記録ファイルは64 MiBまで読み込めます")
    samples = []
    with open(path, "rb") as stream:
        header = json.loads(stream.readline(MAX_PACKET + 1))
        if header != {"format": "kashipan.cpu.capture", "version": 1}:
            raise ValueError("対応していない記録ファイルです")
        while line := stream.readline(MAX_PACKET + 1):
            if len(line) > MAX_PACKET or len(samples) >= MAX_SAMPLES:
                raise ValueError("記録の上限を超えています")
            sample = validate_snapshot(json.loads(line))
            stamp = sample.get("receivedAt")
            if type(stamp) not in (int, float) or not 0 <= stamp <= 4102444800 or not math.isfinite(stamp):
                raise ValueError("不正な記録時刻です")
            if not isinstance(sample.get("session"), str) or len(sample["session"]) > 128:
                raise ValueError("不正なセッションです")
            samples.append(sample)
    if not samples:
        raise ValueError("記録にサンプルがありません")
    return samples
