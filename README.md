# parallel-arch

Two projects from a graduate course on parallel computer architecture (NC State ECE 506, Spring 2026):

1. **`sst_mesi/`** — cache-coherence experiments in the [SST](https://sst-simulator.org) `memHierarchy` element: MESI vs MSI on a 7-pattern bus suite, then a **migratory-sharing optimization** (read miss promoted to GetX) under an inclusive-L2 directory.
2. **`ocean_omp/`** — a 2-D stencil (the "Ocean" temperature simulator) parallelized with OpenMP; thread-scaling sweep across four decompositions.

Every number below comes from a file in this repo; `STATE.md` lists the path for each.

---

## 1. SST: MESI vs MSI, and migratory sharing

### Setup

4 cores, private L1s (`memHierarchy.Cache`, MESI or MSI), one shared bus, one inclusive shared L2, one memory controller. Part 1 drives the caches with Miranda pattern generators; Part 2 drives them with Prospero trace files (`sst_mesi/traces/`). The L1 coherence manager (`MESI_L1.cc`) was instrumented with a per-cache counter bank (`Counters.h`): requests sent, invalidations seen, interventions, and every state transition.

### Part 1 — what the E state buys (sums over the 4 L1s)

| pattern | GetX, MSI → MESI | invalidations | what happened |
|---|---|---|---|
| disjoint read-then-write | **32 → 0** | 0 → 0 | lines are read into **E** and written silently (S→E 32, E→M 32) |
| disjoint read-only | 0 → 0 | 0 | MESI goes I→E, MSI goes I→S; no bus cost either way |
| disjoint writes | 32 → 32 | 0 | writes miss straight to M under both |
| mixed R/W with upgrades | 500 → 500 | 1,234 → 1,234 | shared lines never enter E |
| shared write ping-pong | 1000 → 1000 | 992 → 992 | identical traffic |
| shared read-only, single-line read sharing | 0 → 0 | 0 | pure read sharing |

The E state removes exactly the upgrades the textbook says it should: private data that is read and then written. On every shared-line pattern MESI is bit-identical to MSI. In this bus model the upgrade is cheap in time (simulated time moves by ~1 ns out of 4 µs), so the win is in traffic, not latency. Full tables, including per-core request latency: [`sst_mesi/results/TABLES.md`](sst_mesi/results/TABLES.md).

### Part 2 — migratory sharing: promote the read miss to GetX

Migratory sharing is the pattern where one core reads a line, writes it, and hands it to the next core. Under plain MESI each core pays two bus transactions: a GetS that brings the line in S, then a GetX upgrade (with an invalidation broadcast) when it writes. The change, in `MESI_L1.cc` `handleGetS()`, state-I branch:

```cpp
if (coherence_opt_) {
    // Migratory sharing: promote GetS miss to GetX
    send_time = forwardMessage(event, line_size_, 0, nullptr, Command::GetX);
    incRequestSent(cachename_, Command::GetX);
    line->setState(IM);
} else {
    send_time = forwardMessage(event, line_size_, 0, nullptr);
    incRequestSent(cachename_, Command::GetS);
    line->setState(IS);
}
```

The line arrives directly in M through the existing IM→M response path; the later write is a hit. No new states or transitions were added.

Directed test: 4 cores, one read→write pair each on one line, round-robin.

| | base | optimized |
|---|---|---|
| GetS sent | 4 | **0** |
| GetX sent | 4 | 4 |
| invalidations seen | 6 | **0** |
| interventions | 3 | **0** |
| **bus requests** | **8** | **4** |

**What this does not do.** `coherence_opt` is a static switch: *every* read miss becomes GetX. That is correct for a migratory workload and wrong for a read-shared one, where it would fetch lines exclusively that other cores also want to read and cause needless invalidations. A real implementation needs a per-line migratory detector (e.g. "the last owner read then wrote this line" bits in the directory) that turns the promotion on only for lines that have shown the pattern. The counters are in place to measure such a detector; the detector itself is future work.

One measurement caveat, so nobody repeats it: the `S→M` transition counter reads 0 in the base run too, because the upgrade passes through a transient `SM` state the counter bank does not canonicalise. The evidence for the base upgrades is `GetX = 4` and `S→I = 3`, not that counter.

### Running it

The `src/` directory is a partial copy of SST's `memHierarchy` element: only the files that were changed (`MESI_L1.cc/.h`, `Counters.h`, `cacheFactory.cc`) plus the model scripts and traces. Rebuilding needs `sst-core` and `sst-elements` on Linux; drop these files over the stock element, rebuild, then:

```
cd sst_mesi/src
python3 run-miranda-bus-coherence-suite.py   # Part 1: 7 patterns × {MSI, MESI}
python3 run-prospero-traces.py               # Part 2: base vs optimized
cd .. && python3 parse_results.py            # regenerates results/TABLES.md from the .out files
```

Raw simulator output is kept under `sst_mesi/results/`; `TABLES.md` is generated from it, never edited by hand.

---

## 2. OpenMP: 2-D stencil scaling

A 1026 × 1026 grid of doubles, 5,000 steps of 4-neighbour averaging, two grids swapped each step (two 8.4 MB arrays). Four decompositions: row-static (`schedule(static)`), row-static with 32-row chunks, row-dynamic (`schedule(dynamic)`), and 2-D square blocks.

Measured on a 12-core / 24-thread laptop CPU (AMD Ryzen AI 9 HX 370, WSL2, gcc 15.2 `-O2 -fopenmp`, `OMP_PROC_BIND=true OMP_PLACES=cores`), 5 runs per point, minimum reported; the per-run numbers are in `ocean_omp/_build/summary.csv`.

| threads | row-static | static, 32-row chunks | dynamic | square blocks |
|---|---|---|---|---|
| 1 | 4.436 s / 1.00× | — | — | — |
| 2 | 2.384 / **1.86×** | 1.86× | 1.38× | 1.51× |
| 4 | 1.462 / **3.03×** | 2.86× | 2.50× | 2.74× |
| 8 | 1.146 / **3.87×** | 3.89× | 2.74× | 3.31× |
| 12 | 0.931 / **4.76×** | 4.55× | 2.96× | 3.89× |
| 16 | 0.914 / 4.85× | 4.90× | 3.03× | 4.18× |
| 24 | 0.866 / 5.12× (median 3.09×) | 3.67× | 1.85× | 2.66× |

What the numbers say:

- **Row-static wins at every thread count.** For a row-major stencil it gives each thread contiguous memory and no scheduler overhead.
- **Scaling saturates near 5× on 12 physical cores.** The loop streams two 8.4 MB grids per step, so it is memory-bandwidth-bound; past ~8 cores the extra threads mostly wait on the same DRAM.
- **`schedule(dynamic)` costs about a third of the throughput**: runtime scheduling overhead on roughly 1 ms of work per row.
- **24 threads is worse than 12 on every variant**: two SMT threads per core share one load/store path on a loop that is already bandwidth-bound. The T=24 static row is bimodal (0.87 s minimum, 1.44 s median), so the median is the honest number there.

Sources are in `ocean_omp/src/` (one file per decomposition); `ocean_omp/scripts/run_scaling.sh` reproduces the sweep.

---

## Layout

```
sst_mesi/
  src/         changed SST files, model scripts, run scripts
  traces/      Prospero traces for the directed migratory test
  results/     raw .out files + TABLES.md (generated by parse_results.py)
  docs/        the written report
ocean_omp/
  src/         ocean_*.c, one per decomposition
  scripts/     local sweep (run_scaling.sh) and the cluster bsub scripts
  _build/      summary.csv and the per-run timing CSVs
STATE.md       every measured number with the file it came from
```

## License and attribution

The files under `sst_mesi/src/` are modifications of the SST `memHierarchy` element, copyright NTESS, distributed under SST's BSD-style license; the modifications are released under the same terms. Everything else is my own work for the course and may be used with attribution. The course's own assignment handouts are not included.
