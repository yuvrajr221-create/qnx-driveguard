import json, time, urllib.request
rows = []
t0 = time.time()
while time.time() - t0 < 35:
    try:
        rows.append(json.load(urllib.request.urlopen("http://localhost:8080/api/stats", timeout=2)))
    except Exception:
        pass
    time.sleep(0.25)
lv = [r["level"] for r in rows]
ch = sum(1 for a, b in zip(lv, lv[1:]) if a != b)
print("samples", len(rows), "min_gap", min(r["gap"] for r in rows), "min_ttc", min(r["ttc"] for r in rows),
      "max_level", max(lv), "level_changes", ch, "min_speed", min(r["speed"] for r in rows))
