# CLAUDE.md — parallel_arch

Inherits root `Prometius/CLAUDE.md`. Adds:

## What this is
ECE 506 (NCSU, Spring 2026): Project 1 (Ocean 2-D stencil, OpenMP) and
Project 2 (SST memHierarchy MESI_L1 instrumentation + migratory-sharing
optimisation), reconstituted 2026-09-19 from the course folder and the
submission tarball. `STATE.md` holds every measured number with its path.

## Project-specific rules
- **`sst_mesi/src/` is a partial copy of SST's memHierarchy element**, not a
  buildable tree. It holds only the files Chaitanya changed (`MESI_L1.cc/.h`,
  `Counters.h`, `cacheFactory.cc`) plus the run scripts and traces. Rebuilding
  needs sst-core + sst-elements on Linux; the submission tarball
  `Chaitu/NCSU/spring26/Architecture_of_Parallel_Computers_506/ckunda_proj2p2.tar.gz`
  has the full element.
- **Results are the raw `.out` files.** `sst_mesi/results/TABLES.md` is
  generated from them by `parse_results.py`; regenerate, never hand-edit.
- **Never cite "S→M upgrades 0".** The counter reads 0 in base too. See STATE.md.
- **Ocean numbers are from this laptop (HX 370, WSL), not Henry2.** Say so
  wherever they appear. `scripts/run_scaling.sh` is the local sweep;
  `scripts/run_all.bsub` is the untouched cluster script.

## Run
```
# Ocean sweep (WSL Ubuntu): from ocean_omp/
bash scripts/run_scaling.sh 5 "1 2 4 8 12 16 24"   # -> _build/summary.csv
# or from PowerShell:  ocean_omp\scripts\launch_sweep.ps1

# SST tables: from sst_mesi/
python3 parse_results.py                            # -> results/TABLES.md
```
