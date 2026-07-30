#!/usr/bin/env python3

import argparse
import sys

from strength.runner import run as run_strength

RUNNERS = {"strength": run_strength}

def main():
    parser = argparse.ArgumentParser(description="Knightrider test orchestrator")
    parser.add_argument("test", choices=tuple(RUNNERS))
    parser.add_argument("args", nargs=argparse.REMAINDER, help="arguments forwarded to the test")
    parsed_args = parser.parse_args()

    return RUNNERS[parsed_args.test](parsed_args.args)


if __name__ == "__main__":
    sys.exit(main())
