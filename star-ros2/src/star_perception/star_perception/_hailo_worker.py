#!/usr/bin/env python3
"""Long-lived Hailo-8L YOLOv8 inference worker.

Subprocess run by `HailoYoloRunner` under the Python 3.11 interpreter
in /home/star/hailo-venv when the ROS2 Jazzy system interpreter
(Python 3.12) cannot import `hailo_platform` directly -- HailoRT 4.20
only ships a cp311 aarch64 wheel, and there is no cp312 wheel. On
Raspberry Pi OS Bookworm (Python 3.11) the runner imports
`hailo_platform` in-process and this worker is not spawned.

IPC protocol (length-prefixed binary over stdin/stdout):

  parent -> worker:  <4B big-endian N><N bytes uint8 HxWx3 RGB canvas>
                     (N = input_size * input_size * 3)
  worker -> parent:  <4B big-endian M><M bytes UTF-8 JSON>
                     JSON = {"detections": [[class_id, score,
                                             x_min_n, y_min_n,
                                             x_max_n, y_max_n], ...]}
                     All coords normalized to [0, 1] of the input
                     canvas. Matches the runner's `InferenceBackend`
                     return tuple order.

First message from worker to parent is always the handshake:
  {"status": "ready"}   on successful HEF load
  {"status": "error", "message": "..."}   otherwise
"""

from __future__ import annotations

import argparse
import json
import signal
import struct
import sys
from pathlib import Path

import numpy as np

from hailo_platform import (
    HEF,
    VDevice,
    HailoStreamInterface,
    ConfigureParams,
    InputVStreamParams,
    OutputVStreamParams,
    InferVStreams,
    FormatType,
)


def _write_message(buf: bytes) -> None:
    sys.stdout.buffer.write(struct.pack(">I", len(buf)))
    sys.stdout.buffer.write(buf)
    sys.stdout.buffer.flush()


def _read_exact(n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sys.stdin.buffer.read(n - len(buf))
        if not chunk:
            raise EOFError
        buf.extend(chunk)
    return bytes(buf)


def _decode_hailo_yolov8_output(raw):
    """Flatten Hailo NMS output into list of (class_id, score, x_min_n,
    y_min_n, x_max_n, y_max_n).

    The YOLOv8 HEF from the Hailo Model Zoo emits
    `list[batch][class][(num_dets, 5)]` with each row
    `(y_min, x_min, y_max, x_max, score)` in normalized canvas coords.
    Verified empirically on yolov8s_h8l.hef with ultralytics bus.jpg.
    """
    if isinstance(raw, list) and len(raw) == 1:
        per_class = raw[0]
    elif isinstance(raw, list):
        per_class = raw
    else:
        arr = np.asarray(raw)
        if arr.ndim >= 1 and arr.shape[0] == 1:
            arr = arr[0]
        per_class = list(arr)

    out = []
    for class_id, dets in enumerate(per_class):
        arr = np.asarray(dets)
        if arr.size == 0 or arr.ndim != 2 or arr.shape[1] < 5:
            continue
        for row in arr:
            score = float(row[4])
            if score <= 0.0:
                continue
            y_min = float(row[0]); x_min = float(row[1])
            y_max = float(row[2]); x_max = float(row[3])
            out.append((int(class_id), score, x_min, y_min, x_max, y_max))
    return out


class _Runner:
    def __init__(self, hef_path: Path, input_size: int):
        self.hef_path = hef_path
        self.input_size = input_size
        self.vdevice = None
        self._ng = None
        self._in_name = None
        self._out_name = None
        self._ip = None
        self._op = None
        # `activate()` returns a context manager holding the HW context
        # on the Hailo-8L. Previously we entered it per-frame, which
        # costs ~30-80 ms of PCIe programming and drops sustained
        # throughput from ~100 FPS to ~10 FPS. Keep it alive for the
        # lifetime of the worker (single-model server).
        self._activation = None
        # `InferVStreams` allocates DMA input/output buffers and sets
        # up the vstream pipeline; same story - hold it open.
        self._pipe_ctx = None
        self._pipe = None

    def setup(self) -> None:
        hef = HEF(str(self.hef_path))
        self.vdevice = VDevice()
        cp = ConfigureParams.create_from_hef(
            hef=hef, interface=HailoStreamInterface.PCIe
        )
        self._ng = self.vdevice.configure(hef, cp)[0]
        self._in_name = hef.get_input_vstream_infos()[0].name
        self._out_name = hef.get_output_vstream_infos()[0].name
        self._ip = InputVStreamParams.make(
            self._ng, format_type=FormatType.UINT8)
        self._op = OutputVStreamParams.make(
            self._ng, format_type=FormatType.FLOAT32)

        # Enter both contexts once and remember them so infer() is a
        # hot-path that only does the PCIe DMA + inference.
        self._activation = self._ng.activate(self._ng.create_params())
        self._activation.__enter__()
        self._pipe_ctx = InferVStreams(self._ng, self._ip, self._op)
        self._pipe = self._pipe_ctx.__enter__()

    def infer(self, canvas_hwc: np.ndarray):
        batched = np.expand_dims(canvas_hwc, axis=0)
        raw = self._pipe.infer({self._in_name: batched})
        return _decode_hailo_yolov8_output(raw[self._out_name])

    def shutdown(self) -> None:
        if self._pipe_ctx is not None:
            try:
                self._pipe_ctx.__exit__(None, None, None)
            except Exception:
                pass
            self._pipe_ctx = None
            self._pipe = None
        if self._activation is not None:
            try:
                self._activation.__exit__(None, None, None)
            except Exception:
                pass
            self._activation = None
        if self.vdevice is not None:
            try:
                self.vdevice.release()
            except Exception:
                pass
            self.vdevice = None


def _install_signal_handlers(runner: _Runner) -> None:
    def _handler(signum, frame):
        runner.shutdown()
        sys.exit(0)
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, _handler)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hef", required=True, type=Path)
    parser.add_argument("--input-size", type=int, default=640)
    args = parser.parse_args()

    runner = _Runner(args.hef, args.input_size)
    try:
        runner.setup()
    except Exception as exc:
        _write_message(json.dumps(
            {"status": "error", "message": str(exc)}).encode())
        return 1

    _install_signal_handlers(runner)
    _write_message(json.dumps({"status": "ready"}).encode())

    expected = args.input_size * args.input_size * 3
    try:
        while True:
            header = sys.stdin.buffer.read(4)
            if len(header) != 4:
                break
            n = struct.unpack(">I", header)[0]
            if n != expected:
                _read_exact(n)
                _write_message(json.dumps(
                    {"detections": [],
                     "error": f"expected {expected} bytes, got {n}"}
                ).encode())
                continue
            payload = _read_exact(n)
            canvas = np.frombuffer(payload, dtype=np.uint8).reshape(
                args.input_size, args.input_size, 3
            ).copy()  # hailo_platform requires a writeable buffer
            try:
                dets = runner.infer(canvas)
            except Exception as exc:
                _write_message(json.dumps(
                    {"detections": [], "error": str(exc)}).encode())
                continue
            _write_message(json.dumps({"detections": dets}).encode())
    finally:
        runner.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
