#!/usr/bin/env python3
"""
CraftThingy Digital Innovation - Water Pump Controller CLI & Telemetry Tool
Dedicated tool for controlling the automated water pump, reading PZEM-004T electrical
meters, monitoring dry-run safety, and triggering hardware circuit re-probing.
"""

import argparse
import hashlib
import hmac
import json
import secrets
import sys
import time
import urllib.request
import urllib.error

DEFAULT_SECRET = "a1b2c3d4e5f60718293a4b5c6d7e8f90112233445566778899aabbccddeeff00"

def send_request(host: str, method: str, path: str, key: str, sequence: int, body_data: dict = None) -> dict:
    url = f"http://{host}{path}"
    body_str = json.dumps(body_data) if body_data else ""
    timestamp = str(int(time.time()))
    nonce = secrets.token_hex(8)

    # Compute HMAC-SHA256 signature
    payload = f"{method}\n{path}\n{timestamp}\n{nonce}\n{body_str}".encode("utf-8")
    sig = hmac.new(key.encode("utf-8"), payload, hashlib.sha256).hexdigest()

    headers = {
        "Content-Type": "application/json",
        "X-Signature": sig,
        "X-Timestamp": timestamp,
        "X-Nonce": nonce,
        "X-Sequence": str(sequence),
        "X-Master-Key": key
    }

    req = urllib.request.Request(
        url,
        data=body_str.encode("utf-8") if body_str else None,
        headers=headers,
        method=method
    )

    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            resp_body = resp.read().decode("utf-8")
            try:
                return json.loads(resp_body)
            except json.JSONDecodeError:
                return {"raw": resp_body}
    except urllib.error.HTTPError as e:
        err_msg = e.read().decode("utf-8")
        print(f"[-] HTTP Error {e.code}: {err_msg}", file=sys.stderr)
        sys.exit(1)
    except urllib.error.URLError as e:
        print(f"[-] Connection Error: {e.reason}", file=sys.stderr)
        sys.exit(1)

def print_telemetry(data: dict):
    dev = data.get("data", data)
    if isinstance(dev, dict) and "device" in dev:
        dev = dev["device"]

    schema = dev.get("hardwareSchema", "unknown").upper()
    rise_us = dev.get("riseTimeUs", 0)
    running = dev.get("pumpRunning", False)
    auto_mode = dev.get("autoMode", True)
    interrupted = dev.get("interrupted", False)
    demands_fill = dev.get("tankDemandsFill", False)
    dry_run = dev.get("dryRunAlarm", False)
    run_sec = dev.get("runDurationSec", 0)
    max_sec = dev.get("maxRunTimeSec", 2700)

    has_pzem = dev.get("hasPzem", False)
    voltage = dev.get("voltage", 0.0)
    current = dev.get("current", 0.0)
    power = dev.get("power", 0.0)
    energy = dev.get("energy", 0.0)
    freq = dev.get("frequency", 50.0)
    pf = dev.get("powerFactor", 0.0)
    ac_alert = dev.get("acPowerAlert", False)

    print("=" * 60)
    print(" 💧 CraftThingy Water Pump Controller Telemetry")
    print("=" * 60)

    # Status summary
    if running:
        state_str = "\033[92m[RUNNING - PUMP ACTIVE]\033[0m"
    elif interrupted:
        state_str = "\033[91m[INTERRUPTED / MANUAL STOP]\033[0m"
    else:
        state_str = "\033[90m[STANDBY / OFF]\033[0m"

    mode_str = "AUTO (Float Switch)" if auto_mode else "MANUAL OVERRIDE"
    tank_str = "LOW (Demands Water)" if demands_fill else "SATISFIED (Full)"

    print(f" Pump Status    : {state_str}")
    print(f" Operating Mode : {mode_str}")
    print(f" Water Tank     : {tank_str}")
    print(f" Run Duration   : {run_sec // 60}m {run_sec % 60}s (Safety Max: {max_sec // 60}m)")
    if dry_run:
        print(" ⚠️  SAFETY ALARM : DRY-RUN TIMEOUT TRIPPED! Motor stopped.")
    print("-" * 60)

    # Hardware detection
    print(f" Hardware Schema: {schema} (Rise Time: {rise_us} µs)")
    if "1UF" in schema or "10UF" in schema:
        print(" Detection Mode : RC Boot-Delay Circuit on GPIO 2 (Active)")
    elif "LEGACY" in schema:
        print(" Detection Mode : Legacy Wiring (Float GPIO 3, Relay GPIO 0)")
    print("-" * 60)

    # PZEM Electrical Telemetry
    if has_pzem:
        print(" ⚡ PZEM-004T Electrical Instrumentation:")
        print(f"   Voltage       : {voltage:.1f} V AC ({freq:.1f} Hz)")
        print(f"   Current       : {current:.3f} A RMS")
        print(f"   Active Power  : {power:.1f} W (Power Factor: {pf:.2f})")
        print(f"   Total Energy  : {energy:.3f} kWh")
        pct = min(100, round((power / 1300.0) * 100))
        bar_fill = "#" * (pct // 5)
        bar_empty = "-" * (20 - (pct // 5))
        print(f"   House Budget  : [{bar_fill}{bar_empty}] {pct}% of 1300W limit")
        if ac_alert:
            print("   ⚠️  MAINS ALERT: AC line disconnected or circuit breaker tripped!")
    else:
        print(" ℹ️  PZEM Telemetry : Not connected / Legacy wiring mode")

    print("=" * 60)

def main():
    parser = argparse.ArgumentParser(description="CraftThingy Water Pump Controller Management CLI")
    parser.add_argument("--host", default="192.168.4.1", help="Device IP address (default: 192.168.4.1)")
    parser.add_argument("--key", default=DEFAULT_SECRET, help="Master Secret Key / Bearer Token")
    parser.add_argument("--seq", type=int, default=1, help="Rolling sequence counter (default: 1)")

    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("status", help="Get live water pump and PZEM telemetry")
    subparsers.add_parser("start", help="▶️ Manually start pump (forces relay ON)")
    subparsers.add_parser("stop", help="🚨 Emergency interrupt / Stop pump immediately")
    subparsers.add_parser("auto", help="🔄 Resume AUTO float-switch mode")
    subparsers.add_parser("reset-safety", help="🛡️ Reset dry-run safety alarm")
    subparsers.add_parser("reprobe", help="⚡ Re-probe hardware schema (1uF/10uF RC vs Legacy G3)")

    args = parser.parse_args()

    if args.command == "status":
        res = send_request(args.host, "GET", "/api/v1/device/status", args.key, args.seq)
        print_telemetry(res)

    elif args.command == "start":
        res = send_request(args.host, "POST", "/api/v1/device/command", args.key, args.seq, {"command": "start"})
        print("[+] Pump Manual Start:")
        print(json.dumps(res, indent=2))

    elif args.command == "stop":
        res = send_request(args.host, "POST", "/api/v1/device/command", args.key, args.seq, {"command": "stop"})
        print("[!] 🚨 EMERGENCY INTERRUPT:")
        print(json.dumps(res, indent=2))

    elif args.command == "auto":
        res = send_request(args.host, "POST", "/api/v1/device/command", args.key, args.seq, {"command": "auto"})
        print("[+] Operation Resumed (Auto Mode):")
        print(json.dumps(res, indent=2))

    elif args.command == "reset-safety":
        res = send_request(args.host, "POST", "/api/v1/device/command", args.key, args.seq, {"command": "reset_safety"})
        print("[+] Dry-Run Alarm Reset:")
        print(json.dumps(res, indent=2))

    elif args.command == "reprobe":
        res = send_request(args.host, "POST", "/api/v1/device/command", args.key, args.seq, {"command": "reprobe"})
        print("[+] Hardware Re-Probe:")
        print(json.dumps(res, indent=2))

if __name__ == "__main__":
    main()
