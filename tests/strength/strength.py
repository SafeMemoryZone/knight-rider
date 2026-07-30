import argparse
import os
import shutil
import signal
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "build" / "strength"
TARGET = "Knightrider"
DEFAULT_BOOK = REPO / "books" / "strength_openings.epd"

def parse_args(argv):
    p = argparse.ArgumentParser(prog="run.py strength", description="SPRT strength test via fastchess")
    p.add_argument("--candidate", default=None,
                   help="commit/branch under test (default: working tree, uncommitted changes included)")
    p.add_argument("--baseline", default="master", help="commit/branch to compare against")
    p.add_argument("--openings", default=str(DEFAULT_BOOK), help="path to the opening book")
    p.add_argument("--openings-format", default="epd", choices=("epd", "pgn"))
    p.add_argument("--tc", default="10+0.1")
    p.add_argument("--games", type=int, default=2)
    p.add_argument("--rounds", type=int, default=50000)
    p.add_argument("--concurrency", type=int, default=4)
    p.add_argument("--srand", type=int, default=42)
    p.add_argument("--pgnout", default=str(WORK / "games.pgn"))
    p.add_argument("--fastchess", default="fastchess")
    p.add_argument("--jobs", type=int, default=os.cpu_count())
    return p.parse_args(argv)


def checkout(ref, name):
    src = WORK / f"src-{name}"
    subprocess.run(["git", "worktree", "remove", "--force", str(src)], cwd=REPO, check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    shutil.rmtree(src, ignore_errors=True)
    subprocess.run(["git", "worktree", "add", "--detach", str(src), ref], cwd=REPO, check=True)
    return src


def build(ref, name, jobs):
    if ref is None:
        src, bdir = REPO, WORK / "build-local"
    else:
        src, bdir = checkout(ref, name), WORK / f"build-{name}"

    subprocess.run(["cmake", "-S", str(src), "-B", str(bdir), "-DCMAKE_BUILD_TYPE=Release"], check=True)
    subprocess.run(["cmake", "--build", str(bdir), "-j", str(jobs)], check=True)

    binary = WORK / name
    shutil.copy2(bdir / TARGET, binary)
    return binary


def main(argv):
    args = parse_args(argv)

    openings = Path(args.openings).resolve()
    if not openings.is_file():
        print(f"opening book not found: {openings}", file=sys.stderr)
        return 1

    WORK.mkdir(parents=True, exist_ok=True)
    baseline = build(args.baseline, "baseline", args.jobs)
    candidate = build(args.candidate, "candidate", args.jobs)

    cmd = [
        args.fastchess,
        "-engine", f"cmd={candidate}", "name=candidate",
        "-engine", f"cmd={baseline}", "name=baseline",
        "-each", f"tc={args.tc}",
        "-openings", f"file={openings}", f"format={args.openings_format}", "order=random",
        "-srand", str(args.srand),
        "-games", str(args.games),
        "-rounds", str(args.rounds),
        "-sprt", "elo0=0", "elo1=10", "alpha=0.05", "beta=0.05", "model=normalized",
        "-concurrency", str(args.concurrency),
        "-pgnout", f"file={Path(args.pgnout).resolve()}", "notation=san", "append=false",
        "-strict",
    ]
    proc = subprocess.Popen(cmd, cwd=WORK)
    while True:
        try:
            return proc.wait()
        except KeyboardInterrupt:
            proc.send_signal(signal.SIGINT)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
