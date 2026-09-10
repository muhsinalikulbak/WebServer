#!/usr/bin/env python3
import sys
import os

method = os.environ.get("REQUEST_METHOD", "GET")
cl = os.environ.get("CONTENT_LENGTH", "0")
ct = os.environ.get("CONTENT_TYPE", "(none)")

body = ""
try:
    length = int(cl or "0")
    body = sys.stdin.read(length) if length > 0 else ""
except Exception as e:
    body = "ERR:" + str(e)

print("Status: 200 OK")
print("Content-Type: text/plain")
print("")
print("METHOD=" + method)
print("CONTENT_LENGTH=" + str(cl))
print("CONTENT_TYPE=" + ct)
print("BODY_LEN=" + str(len(body)))
print("BODY=[" + body + "]")