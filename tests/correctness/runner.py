import argparse
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = Path(__file__).resolve().parent
WORK = REPO / "build" / "correctness"
TARGET = "perft"

def parse_args(argv):
    p = argparse.ArgumentParser(prog="run.py correctness", description="perft correctness test via doctest")
    p.add_argument("--jobs", type=int, default=os.cpu_count())
    p.add_argument("doctest_args", nargs=argparse.REMAINDER, help="arguments forwarded to doctest")
    return p.parse_args(argv)


def build(jobs):
    subprocess.run(["cmake", "-S", str(SRC), "-B", str(WORK), "-DCMAKE_BUILD_TYPE=Release"], check=True)
    subprocess.run(["cmake", "--build", str(WORK), "-j", str(jobs)], check=True)
    return WORK / TARGET


def main(argv):
    args = parse_args(argv)

    WORK.mkdir(parents=True, exist_ok=True)
    binary = build(args.jobs)

    return subprocess.run([str(binary), *args.doctest_args]).returncode


def run(args=None):
    return main(args or [])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
