#!/usr/bin/env python3

import random
import argparse

maxint = 1 << 31

parser = argparse.ArgumentParser(description = "Generate random numbers file")
parser.add_argument('-f', type=str, required=True, help="file name")
parser.add_argument('-r', type=str, required=False, help='file name for sorted numbers')
parser.add_argument('-c', type=int, required=True, help='number count')
parser.add_argument('-m', type=int, default=maxint, help='maximal number')
args = parser.parse_args()
random.seed()

def write_numbers(filename, numbers, count):
    with open(filename, 'w') as f:
        for i, n in enumerate(numbers):
            f.write(str(n))
            if i + 1 != count:
                f.write(' ')


if args.r:
    numbers = [
        random.randint(0, args.m) for _ in range(0, args.c)
    ]
else:
    numbers = (
        random.randint(0, args.m) for _ in range(0, args.c)
    )

write_numbers(args.f, numbers, args.c)

if args.r:
    write_numbers(args.r, sorted(numbers), args.c)