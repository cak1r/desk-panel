"""
Reads CPU/RAM usage via psutil and (if an NVIDIA GPU is present) GPU
utilization via pynvml, and publishes them to MQTT for the desk panel's
PC health tile.
"""

import json
import threading

import psutil

import config

try:
    import pynvml
except ImportError:
    pynvml = None


def health_publish_loop(client, stop_event: threading.Event) -> None:
    gpu_handle = None
    if pynvml is not None:
        try:
            pynvml.nvmlInit()
            gpu_handle = pynvml.nvmlDeviceGetHandleByIndex(0)
        except Exception as exc:  # no NVIDIA GPU, driver not loaded, etc.
            print(f"NVML unavailable, GPU reading disabled: {exc}")

    try:
        while not stop_event.is_set():
            try:
                payload = {
                    "cpu": round(psutil.cpu_percent(interval=1)),
                    "ram": round(psutil.virtual_memory().percent),
                }
                if gpu_handle is not None:
                    try:
                        util = pynvml.nvmlDeviceGetUtilizationRates(gpu_handle)
                        payload["gpu"] = round(util.gpu)
                    except Exception as exc:
                        print(f"GPU read error: {exc}")
                client.publish(config.TOPIC_HEALTH, json.dumps(payload), qos=1, retain=True)
            except Exception as exc:  # psutil calls can transiently fail
                print(f"Health publish error: {exc}")
            stop_event.wait(max(0.0, config.HEALTH_PUBLISH_INTERVAL_S - 1))
    finally:
        if gpu_handle is not None:
            try:
                pynvml.nvmlShutdown()
            except Exception:
                pass
