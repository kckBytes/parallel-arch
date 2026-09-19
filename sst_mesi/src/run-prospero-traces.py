#!/usr/bin/env python3

import os
import subprocess
from pathlib import Path


def filter_custom_l1_counter_lines(log_text: str) -> str:
    lines = []
    for line in log_text.splitlines():
        if "Counter[" in line and "inv=" in line:
            lines.append(line)
    return "\n".join(lines) + ("\n" if lines else "")


def run_case(sdl_path: Path, opt_on: bool, env_base: dict, out_name: str) -> None:
    env = dict(env_base)
    env["OPT_ON"] = "1" if opt_on else "0"

    out_path = sdl_path.parent / out_name
    res = subprocess.run(
        ["sst", str(sdl_path)],
        cwd=str(sdl_path.parent),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    if res.returncode != 0:
        print(res.stdout, end="")
        raise subprocess.CalledProcessError(res.returncode, res.args, output=res.stdout)
    out_path.write_text(filter_custom_l1_counter_lines(res.stdout), encoding="utf-8")


def main() -> int:
    base_dir = Path(__file__).resolve().parent
    sdl_path = base_dir / "example-4core-prospero.py"
    if not sdl_path.exists():
        raise FileNotFoundError(f"Missing SDL: {sdl_path}")

    protocol = "MESI"

    # Directory containing the editable trace files:
    #   prospero-traces/prospero_core0.trace ... prospero_core3.trace
    trace_dir = base_dir / "prospero-traces"
    if not trace_dir.exists():
        raise FileNotFoundError(
            f"Trace dir not found: {trace_dir}. Create/adjust it or run the generating script first."
        )

    env_base = os.environ.copy()
    env_base["PROSPERO_TRACE_DIR"] = str(trace_dir)

    for opt_on in (False, True):
        out_name = f"prospero_{protocol.lower()}_{'opt' if opt_on else 'base'}.out"
        print(f"[prospero] run protocol={protocol} {'opt' if opt_on else 'base'} -> {out_name}")
        run_case(sdl_path, opt_on=opt_on, env_base=env_base, out_name=out_name)

    print("[prospero] done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

