#!/usr/bin/env bash
# =============================================================================
# run_scaling.sh -- Ocean OpenMP thread-scaling sweep, local (WSL) version of
# scripts/run_all.bsub (the Henry2 / LSF script that was never run, or whose
# output was never saved -- data/times_template_as_found.csv is empty).
#
# Usage (from WSL):  bash run_scaling.sh [reps] [thread list]
#   default reps = 5, threads = "1 2 4 8 12"
#
# Same grid, same size, same steps as the .bsub: 1026 x 1026, 5000 steps,
# grid1026_hotTop.txt. Four variants, as in run_all.bsub:
#   static    schedule(static)
#   static32  schedule(static, 32)
#   dynamic   schedule(dynamic)
#   square    2-D blocked decomposition
# Output: _build/times_<variant>.csv  (one row per rep, one column per T)
#         _build/summary.csv          (min / median per variant x T, speedup vs T=1)
#         _build/raw_<variant>_T<n>_r<i>.txt (the program's own stdout)
#
# Timing is the program's own "Time = ... seconds" (omp_get_wtime around the
# time-step loop), so file I/O is excluded -- same as the .bsub would report.
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")/.."
REPS="${1:-5}"
THREADS="${2:-1 2 4 8 12}"
SIZE=1026; STEPS=5000; GRID=data/grid1026_hotTop.txt
mkdir -p _build

echo "== host: $(hostname)  nproc=$(nproc)  gcc=$(gcc -dumpversion)"
lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core\(s\)" | sed 's/^/   /'

for v in static static32 dynamic square; do
  gcc -O2 -fopenmp -o _build/ocean_$v src/ocean_$v.c -lm
done
echo "== built static static32 dynamic square"

declare -A T_of
for v in static static32 dynamic square; do
  csv=_build/times_$v.csv
  echo -n "rep" > $csv; for T in $THREADS; do echo -n ",T$T" >> $csv; done; echo >> $csv
  for r in $(seq 1 $REPS); do
    echo -n "$r" >> $csv
    for T in $THREADS; do
      # the .bsub only runs the non-static variants for T>1; keep that
      if [[ "$v" != "static" && "$T" == "1" ]]; then echo -n "," >> $csv; continue; fi
      raw=_build/raw_${v}_T${T}_r${r}.txt
      OMP_NUM_THREADS=$T OMP_PROC_BIND=true OMP_PLACES=cores ./_build/ocean_$v $SIZE $STEPS $GRID /dev/null > $raw 2>&1
      t=$(grep -oE "Time = [0-9.]+" $raw | awk '{print $3}')
      echo -n ",$t" >> $csv
      printf "   %-9s T=%-2s rep %s : %s s\n" $v $T $r $t
    done
    echo >> $csv
  done
done

python3 - "$THREADS" <<'EOF'
import csv, sys, statistics as st
threads=sys.argv[1].split()
rows=[["variant","T","n","min_s","median_s","speedup_min_vs_static_T1","speedup_median_vs_static_T1"]]
base_min=base_med=None
for v in ["static","static32","dynamic","square"]:
    with open(f"_build/times_{v}.csv") as f:
        r=list(csv.DictReader(f))
    for T in threads:
        vals=[float(x[f"T{T}"]) for x in r if x[f"T{T}"]]
        if not vals: continue
        mn, md = min(vals), st.median(vals)
        if v=="static" and T=="1": base_min, base_med = mn, md
        rows.append([v,T,len(vals),f"{mn:.4f}",f"{md:.4f}",
                     f"{base_min/mn:.2f}" if base_min else "", f"{base_med/md:.2f}" if base_med else ""])
with open("_build/summary.csv","w",newline="") as f: csv.writer(f).writerows(rows)
w=[max(len(str(c)) for c in col) for col in zip(*rows)]
for row in rows: print("   "+"  ".join(str(c).ljust(w[i]) for i,c in enumerate(row)))
EOF
echo "== done: _build/summary.csv"
