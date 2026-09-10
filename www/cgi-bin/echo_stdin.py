#!/usr/bin/env python3
import sys

data = sys.stdin.read()
total = len(data)
print("Status: 200 OK")
print("Content-Type: text/plain")
print("")
print("READ_BYTES=" + str(total))
print("CONTENT=" + data)