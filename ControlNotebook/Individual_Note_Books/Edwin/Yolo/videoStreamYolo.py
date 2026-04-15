from flask import Flask, Response
from picamera2 import Picamera2
import cv2
import numpy as np
import threading
import os
import atexit
import RPi.GPIO as GPIO

app = Flask(__name__)

# ── Camera setup ──────────────────────────────────────────────────────────────
camera = Picamera2()
camera.configure(camera.create_preview_configuration(
    main={"format": "RGB888", "size": (640, 480)}
))
camera.start()

# ── YOLOv4-tiny setup (OpenCV DNN — no extra packages needed) ─────────────────
BASE = os.path.dirname(os.path.abspath(__file__))
net = cv2.dnn.readNet(
    os.path.join(BASE, 'yolov4-tiny.weights'),
    os.path.join(BASE, 'yolov4-tiny.cfg'),
)
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

with open(os.path.join(BASE, 'coco.names')) as f:
    CLASS_NAMES = [line.strip() for line in f]

OUTPUT_LAYERS = [net.getLayerNames()[i - 1]
                 for i in net.getUnconnectedOutLayers().flatten()]

CONF_THRESH = 0.4
NMS_THRESH  = 0.4

YOLO_ENABLED = True  # Set to False to stream raw video without detection

# ── GPIO setup ────────────────────────────────────────────────────────────────
PERSON_PIN = 17  # BCM pin number — change to whichever pin you have wired

GPIO.setmode(GPIO.BCM)
GPIO.setup(PERSON_PIN, GPIO.OUT, initial=GPIO.LOW)
atexit.register(GPIO.cleanup)  # ensure pin is LOW and freed on exit

# ── Shared state ──────────────────────────────────────────────────────────────
_lock            = threading.Lock()
_latest_raw      = None   # newest BGR frame from camera
_latest_annotated = None  # newest YOLO-annotated frame


def run_yolo(frame):
    """Run YOLOv4-tiny on a BGR frame; return (annotated frame, person_detected)."""
    h, w = frame.shape[:2]
    blob = cv2.dnn.blobFromImage(frame, 1/255.0, (416, 416),
                                 swapRB=False, crop=False)
    net.setInput(blob)
    layer_outputs = net.forward(OUTPUT_LAYERS)

    boxes, confidences, class_ids = [], [], []
    for output in layer_outputs:
        for det in output:
            scores = det[5:]
            cid    = int(np.argmax(scores))
            conf   = float(scores[cid])
            if conf < CONF_THRESH:
                continue
            cx, cy, bw, bh = (det[:4] * np.array([w, h, w, h])).astype(int)
            x = cx - bw // 2
            y = cy - bh // 2
            boxes.append([x, y, bw, bh])
            confidences.append(conf)
            class_ids.append(cid)

    indices = cv2.dnn.NMSBoxes(boxes, confidences, CONF_THRESH, NMS_THRESH)

    out = frame.copy()
    person_detected = False
    if len(indices) > 0:
        for i in indices.flatten():
            x, y, bw, bh = boxes[i]
            label = f"{CLASS_NAMES[class_ids[i]]}: {confidences[i]:.2f}"
            cv2.rectangle(out, (x, y), (x + bw, y + bh), (0, 255, 0), 2)
            cv2.putText(out, label, (x, y - 5),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
            if CLASS_NAMES[class_ids[i]] == 'person':
                person_detected = True
    return out, person_detected


def detection_loop():
    """Background thread: runs YOLO as fast as the Pi can manage."""
    global _latest_annotated
    while True:
        if not YOLO_ENABLED:
            continue
        with _lock:
            frame = _latest_raw
        if frame is None:
            continue
        annotated, person_detected = run_yolo(frame)
        GPIO.output(PERSON_PIN, GPIO.HIGH if person_detected else GPIO.LOW)
        with _lock:
            _latest_annotated = annotated


threading.Thread(target=detection_loop, daemon=True).start()


# ── MJPEG stream ──────────────────────────────────────────────────────────────
def generate_frames():
    global _latest_raw
    while True:
        raw = camera.capture_array()
        bgr = cv2.cvtColor(raw, cv2.COLOR_RGB2BGR)

        with _lock:
            _latest_raw = bgr
            output = (_latest_annotated if YOLO_ENABLED and _latest_annotated is not None
                      else bgr)

        _, buffer = cv2.imencode('.jpg', output)
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')


@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
