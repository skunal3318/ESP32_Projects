from flask import Flask, request, jsonify, render_template, abort
from flask_cors import CORS
import sqlite3
import threading
import time
from datetime import datetime, timedelta

DB = "devices.db"
OFFLINE_TIMEOUT_SECONDS = 90  # mark offline if not seen in 90 seconds

app = Flask(__name__, template_folder="templates", static_folder="static")
CORS(app)


def init_db():
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS devices (
            device_name TEXT PRIMARY KEY,
            ip TEXT,
            status TEXT,
            last_seen TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()


def upsert_device(device_name: str, ip: str, status: str):
    now = datetime.utcnow().isoformat()
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute("""
        INSERT INTO devices(device_name, ip, status, last_seen)
        VALUES(?, ?, ?, ?)
        ON CONFLICT(device_name) DO UPDATE SET
            ip=excluded.ip,
            status=excluded.status,
            last_seen=excluded.last_seen
    """, (device_name, ip, status, now))
    conn.commit()
    conn.close()


def get_all_devices():
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute("SELECT device_name, ip, status, last_seen FROM devices ORDER BY device_name")
    rows = c.fetchall()
    conn.close()
    devices = []
    for name, ip, status, last_seen in rows:
        devices.append({
            "device_name": name,
            "ip": ip,
            "status": status,
            "last_seen": last_seen
        })
    return devices


def delete_device(name):
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute("DELETE FROM devices WHERE device_name = ?", (name,))
    conn.commit()
    conn.close()


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/register", methods=["POST"])
def register():
    data = request.get_json(force=True)
    if not data:
        return jsonify({"error": "invalid json"}), 400
    device_name = data.get("device_name")
    ip = data.get("ip")
    status = data.get("status", "online")
    if not device_name or not ip:
        return jsonify({"error": "device_name and ip required"}), 400
    try:
        upsert_device(device_name, ip, status)
        return jsonify({"ok": True}), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/devices", methods=["GET"])
def devices():
    devices = get_all_devices()
    return jsonify({"devices": devices}), 200


@app.route("/delete/<device_name>", methods=["DELETE"])
def delete(device_name):
    # Careful: device_name is taken as raw string. For production sanitize.
    delete_device(device_name)
    return jsonify({"ok": True}), 200


def cleanup_loop():
    """Background thread: marks devices offline if last_seen older than threshold."""
    while True:
        threshold = datetime.utcnow() - timedelta(seconds=OFFLINE_TIMEOUT_SECONDS)
        conn = sqlite3.connect(DB)
        c = conn.cursor()
        c.execute("SELECT device_name, last_seen FROM devices")
        rows = c.fetchall()
        for name, last_seen in rows:
            if not last_seen:
                continue
            try:
                last = datetime.fromisoformat(last_seen)
            except Exception:
                continue
            if last < threshold:
                # mark offline
                c.execute("UPDATE devices SET status = ? WHERE device_name = ?", ("offline", name))
        conn.commit()
        conn.close()
        time.sleep(15)


if __name__ == "__main__":
    init_db()
    # start cleanup thread
    t = threading.Thread(target=cleanup_loop, daemon=True)
    t.start()
    # listen on all interfaces so ESP32 on LAN can reach it. Set debug=False for production.
    app.run(host="0.0.0.0", port=5000, debug=True)
