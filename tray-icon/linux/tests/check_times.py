#!/usr/bin/env python3
"""check_times.py — the Python port must agree with the page, to the minute.

Runs prayTimes() from "html file/start-dock.html" under node and pray_times()
from achma_tray/prayer_core.py for every city on every day of a year, with a
UTC+1 / UTC+0 switch to exercise the tz argument, and diffs the two.

    python3 tests/check_times.py [year]        (needs node)
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
from achma_tray.prayer_core import CITIES, pray_times, hm  # noqa: E402

year = int(sys.argv[1]) if len(sys.argv) > 1 else 2026
page = open(os.path.join(HERE, "..", "..", "..", "html file", "start-dock.html"), encoding="utf-8").read()
a = page.index("const prayTimes=(()=>{")
b = page.index("})();", a) + 5
days = [(m, d) for m in range(1, 13) for d in range(1, 32)
        if not (m == 2 and d > (29 if year % 4 == 0 else 28)) and not (d == 31 and m in (4, 6, 9, 11))]
tz = lambda m: 1 if 3 <= m <= 10 else 0

js = 'const P=n=>String(n).padStart(2,"0");\n' + page[a:b] + """
const C=%s, D=%s, Y=%d, out=[];
for(const [id,lat,lng] of C) for(const [m,d] of D){
  const t=prayTimes(Y,m,d,lat,lng,(m>=3&&m<=10)?1:0);
  out.push([id,m,d,t.Fajr,t.Sunrise,t.Dhuhr,t.Asr,t.Maghrib,t.Isha].join(' '));}
console.log(out.join('\\n'));""" % ([[c[0], c[3], c[4]] for c in CITIES], [list(x) for x in days], year)
want = subprocess.run(["node", "-e", js], capture_output=True, text=True, check=True).stdout.split("\n")

got = []
for cid, _ar, _fr, lat, lng in CITIES:
    for m, d in days:
        t = pray_times(year, m, d, lat, lng, tz(m))
        got.append(" ".join([str(cid), str(m), str(d)] + [hm(x) for x in t]))

bad = [(w, g) for w, g in zip(want, got) if w.strip() != g]
n = len(got) * 6
if bad or len(want) < len(got):
    print("MISMATCH: %d of %d rows differ" % (len(bad), len(got)))
    for w, g in bad[:10]:
        print("  page:", w, "\n  port:", g)
    sys.exit(1)
print("identical: %d cities x %d days = %d prayer times" % (len(CITIES), len(days), n))
