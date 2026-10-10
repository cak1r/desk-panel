"""
CLI to bump today's Claude usage counters from any script or shell.

Example:
    python record_usage.py --tokens 1532
    python record_usage.py --tokens 812 --requests 1
"""

import argparse

from usage_store import record_usage


def main() -> None:
    parser = argparse.ArgumentParser(description="Record Claude API usage for the desk panel.")
    parser.add_argument("--tokens", type=int, default=0, help="Tokens used by this call (input + output).")
    parser.add_argument("--requests", type=int, default=1, help="Number of requests this call represents.")
    args = parser.parse_args()

    totals = record_usage(tokens=args.tokens, requests=args.requests)
    print(f"Today: {totals['requests_today']} requests, {totals['tokens_today']} tokens")


if __name__ == "__main__":
    main()
