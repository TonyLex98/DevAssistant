#!/usr/bin/env python3
"""Download the OpenSSL 1.1.1 DLLs that Qt 5.15 loads for HTTPS (Windows x64).

Qt 5.15 works only with OpenSSL 1.1 (not 3.x) and no longer gets it from the Qt installer, so
the build takes the conda-forge package, checks its SHA-256 against the channel's metadata,
and extracts libssl-1_1-x64.dll and libcrypto-1_1-x64.dll.

    python scripts/fetch_openssl.py <output folder>
"""
import hashlib
import io
import json
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

VERSION = "1.1.1w"
WANTED = ("libssl-1_1-x64.dll", "libcrypto-1_1-x64.dll")


def zstd_stream(data: bytes):
    """A readable stream of zstd-compressed bytes (Python 3.14+, or the zstandard package)."""
    try:
        from compression import zstd  # Python 3.14+
        return zstd.ZstdFile(io.BytesIO(data))
    except ImportError:
        import zstandard
        return zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data))


def package_files(basename: str, data: bytes):
    """Yields (file name, content) for the files of a conda package (.tar.bz2 or .conda)."""
    if basename.endswith(".tar.bz2"):
        tars = [tarfile.open(fileobj=io.BytesIO(data), mode="r:bz2")]
    else:
        # .conda: a zip holding pkg-*.tar.zst with the files and info-*.tar.zst with metadata.
        archive = zipfile.ZipFile(io.BytesIO(data))
        inner = [n for n in archive.namelist() if n.startswith("pkg-") and n.endswith(".tar.zst")]
        tars = [tarfile.open(fileobj=zstd_stream(archive.read(n)), mode="r|") for n in inner]
    for tar in tars:
        for member in tar:
            if member.isfile():
                yield member.name.rsplit("/", 1)[-1], tar.extractfile(member).read()


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "openssl")
    out.mkdir(parents=True, exist_ok=True)

    with urllib.request.urlopen("https://api.anaconda.org/package/conda-forge/openssl/files", timeout=120) as r:
        files = json.load(r)
    candidates = [f for f in files
                  if f.get("version") == VERSION
                  and f.get("attrs", {}).get("subdir") == "win-64"
                  and f.get("basename", "").endswith((".tar.bz2", ".conda"))]
    if not candidates:
        print(f"No conda-forge package for OpenSSL {VERSION} on win-64", file=sys.stderr)
        return 1
    package = sorted(candidates, key=lambda f: f.get("upload_time", ""))[-1]
    url = package["download_url"]
    if url.startswith("//"):
        url = "https:" + url

    with urllib.request.urlopen(url, timeout=300) as r:
        data = r.read()
    digest = hashlib.sha256(data).hexdigest()
    if package.get("sha256") and digest != package["sha256"]:
        print(f"SHA-256 mismatch for {url}", file=sys.stderr)
        return 1

    found = []
    for name, content in package_files(package["basename"], data):
        if name in WANTED:
            (out / name).write_bytes(content)
            found.append(name)
    if sorted(found) != sorted(WANTED):
        print(f"Expected {WANTED}, found {found}", file=sys.stderr)
        return 1
    print(f"OpenSSL {VERSION} from {package['basename']} (sha256 {digest[:16]}...) -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
