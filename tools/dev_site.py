#!/usr/bin/env python3
"""Builds a local, TEST-key-signed copy of the pack catalog so the app can be run and
screenshotted without the real signing key. Needs the `cryptography` package.

  python3 tools/dev_site.py ../jobeval-data/dist OUTDIR      # then serve OUTDIR over HTTP
  export JOBEVAL_CATALOG_URL=http://127.0.0.1:8765/
  export JOBEVAL_DEV_PUBKEY=$(cat evalcore/tests/fixtures/test.pub)

`../jobeval-data/dist` comes from `python3 tools/build.py compile && ... manifest` in the
data repo. The test key protects nothing; the app titles itself "[DEV KEY]" when overridden.
"""
import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "evalcore" / "tests"))
import make_fixtures as mf  # noqa: E402


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copytree(src / "packs", out / "packs")
    index = (src / "index.json").read_text()
    (out / "index.json").write_text(index)
    generated = json.loads(index)["generated"]
    (out / "index.json.minisig").write_text(mf.minisig(index.encode(), "generated:" + generated))
    print("wrote", out)


if __name__ == "__main__":
    main()
