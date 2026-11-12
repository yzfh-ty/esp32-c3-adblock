#!/usr/bin/env python3
"""生成固定版本的中文网页烧录组件；只修改文案，不修改烧录逻辑。"""

import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
VERSION = "10.1.1"
URL = f"https://registry.npmjs.org/esp-web-tools/-/esp-web-tools-{VERSION}.tgz"
SHA256 = "a67997f41505a55edcf3ad888a0c4fd8fc77395b991f29daebfc8e3bbc646691"
DEST = ROOT / "docs" / "vendor" / "esp-web-tools"


def main():
    translations = json.loads(
        Path(__file__).with_name("web_installer_zh.json").read_text(encoding="utf-8")
    )
    with urllib.request.urlopen(URL, timeout=60) as response:
        archive = response.read()
    if hashlib.sha256(archive).hexdigest() != SHA256:
        raise ValueError("上游组件校验失败，未写入任何文件")

    # Only read known flat bundle files. Never extract arbitrary archive paths.
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as package:
        files = {
            Path(member.name).name: package.extractfile(member).read().decode("utf-8")
            for member in package.getmembers()
            if member.isfile()
            and member.name.startswith("package/dist/web/")
            and member.name.endswith(".js")
            and "/" not in member.name.removeprefix("package/dist/web/")
        }
        license_text = package.extractfile("package/LICENSE").read().decode("utf-8")

    replacement_count = 0
    for kind, entries in translations.items():
        for old, new in entries:
            if kind == "text":
                pattern = r"(?<=>)\s*" + r"\s+".join(map(re.escape, old.split())) + r"\s*(?=<)"
            elif kind == "phrase":
                pattern = r"\s+".join(map(re.escape, old.split()))
            else:
                raise ValueError(f"未知翻译类型：{kind}")
            count = 0
            for name, source in files.items():
                files[name], matches = re.subn(pattern, lambda _: new, source)
                count += matches
            if not count:
                raise ValueError(f"翻译未匹配，未写入文件：{old}")
            replacement_count += count

    # Check the complete translation set before changing the checked-in bundles.
    DEST.mkdir(parents=True, exist_ok=True)
    notice = f"/* ESP Web Tools {VERSION}: Chinese UI modifications. See README.md and LICENSE. */\n"
    for name, source in sorted(files.items()):
        (DEST / name).write_text(notice + source, encoding="utf-8", newline="\n")
    (DEST / "LICENSE").write_text(license_text, encoding="utf-8", newline="\n")
    print(f"已生成 {len(files)} 个组件文件，替换 {replacement_count} 处文案：{DEST}")


if __name__ == "__main__":
    main()
