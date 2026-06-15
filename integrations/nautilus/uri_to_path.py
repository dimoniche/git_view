#!/usr/bin/env python3
"""Convert file paths and file:// URIs to local filesystem paths."""

import sys
from urllib.parse import unquote, urlparse


def uri_to_path(value: str) -> str:
    value = value.strip()
    if not value:
        return value

    if value.startswith(":/"):
        return value[1:]

    if not value.lower().startswith("file:"):
        return value

    parsed = urlparse(value)
    if parsed.path:
        return unquote(parsed.path)

    remainder = value[5:]
    if remainder.startswith("//"):
        remainder = remainder[2:]
    elif remainder.startswith("/"):
        remainder = remainder[1:]
    return unquote(remainder)


if __name__ == "__main__":
    print(uri_to_path(sys.argv[1]))
