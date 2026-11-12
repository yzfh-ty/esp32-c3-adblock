#!/usr/bin/env python3
"""把 hosts/域名拦截列表预处理成排序后的截断 FNV-1a 哈希数据块，
供 ESP32-C3 广告拦截器使用。哈希存储在 flash 中并在设备上二分查找，
因此无需 PSRAM。

HASH_BYTES 必须与固件（src/main.cpp）一致。5 字节（40 位）在最多约 50 万
域名时能保持约 0 碰撞，且能装下不到 3 MB 的 50 万个。

用法：build_blocklist.py [out.bin] [src ...]
  src = 本地文件或 URL。不指定时，下载一份均衡的日常驱动列表
  （StevenBlack base + Hagezi Light + anti-AD 纯域名列表）约 22 万条，
  在通用广告/追踪/恶意软件拦截的基础上补充中文区规则。

  想要激进的“测试极限”构建（约 50 万，也会拦截社交/通讯）：
    build_blocklist.py blocklist.bin \\
      https://raw.githubusercontent.com/StevenBlack/hosts/master/alternates/fakenews-gambling-porn-social/hosts \\
      https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/ultimate-onlydomains.txt
"""
import re
import sys, os, math, urllib.request

HASH_BYTES = 5                          # 40-bit hashes -- must match firmware
MASK = (1 << (HASH_BYTES * 8)) - 1
FNV_OFFSET = 0xcbf29ce484222325
FNV_PRIME  = 0x100000001b3
U64 = (1 << 64) - 1

# Daily driver that FITS alongside dual-OTA firmware slots (~250k domain budget):
# General ads/trackers/malware plus anti-AD's Chinese-region coverage.
# Around 220k entries / 1.1 MB; the weekly workflow checks the size before release.
# (Hagezi's wildcard lists drop subdomains the firmware's parent-matching already covers).
# Keep the combined list within the filesystem budget when changing sources.
DEFAULT_SOURCES = [
    'https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts',            # base: ads + malware
    'https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/light-onlydomains.txt',  # Hagezi Light (wildcard = domain + subdomains)
    'https://raw.githubusercontent.com/privacy-protection-tools/anti-AD/master/anti-ad-domains.txt',  # Chinese-region DNS rules; no URL/cosmetic syntax
]

# ||domain^  or  @@||domain^  optionally followed by $modifiers
ADG = re.compile(r'^(@@)?\|\|([a-z0-9._-]+)\^?(\$.*)?$', re.I)

ALLOW_MISSING = False

def fnv(b: bytes) -> int:
    h = FNV_OFFSET
    for c in b:
        h = ((h ^ c) * FNV_PRIME) & U64
    return h & MASK                      # truncate to HASH_BYTES

def norm(d: str) -> str:
    d = d.strip().lower().lstrip('*').lstrip('.').rstrip('.')
    return d[4:] if d.startswith('www.') else d

def read_source(src: str) -> str:
    if os.path.exists(src):
        return open(src, errors='ignore').read()
    print(f'  正在下载 {src} ...', file=sys.stderr)
    return urllib.request.urlopen(src, timeout=180).read().decode('utf-8', 'ignore')

def main():
    global ALLOW_MISSING
    args = [a for a in sys.argv[1:] if a != '--allow-missing']
    ALLOW_MISSING = '--allow-missing' in sys.argv[1:]
    out = args[0] if args else 'blocklist.bin'
    sources = args[1:] if len(args) > 1 else DEFAULT_SOURCES

    domains, allow = set(), set()
    skipped = 0
    for src in sources:
        try:
            data = read_source(src)
        except Exception as e:
            # Fail loudly: silently dropping a source shrinks the list without anyone noticing
            # (happened when Hagezi retired domains/light.txt). Pass --allow-missing to continue.
            print(f'  !! 无法读取 {src}：{e}', file=sys.stderr)
            if not ALLOW_MISSING: sys.exit(1)
            continue
        for line in data.splitlines():
            s = line.strip()
            if s.startswith(('||', '@@')):
                line = s                   # AdGuard/ABP syntax: never treat as hosts comment
            elif not s.startswith('#') and ('##' in s or '#@#' in s or '#?#' in s or '#%#' in s):
                skipped += 1; continue    # cosmetic rule (baidu.com##div.ad) — the old
                                          # split('#') heuristic ate '##' and added the
                                          # bare domain (baidu.com!) to the blocklist
            else:
                line = s.split('#', 1)[0].strip()
            if not line or line[0] in '!/[':
                continue
            # AdGuard / ABP basic rules: ||example.com^  and allow rules @@||example.com^
            m = ADG.match(line)
            if m:
                if m.group(3):            # has $modifiers (client/dnstype/etc.) -> can't express, skip
                    skipped += 1; continue
                (allow if m.group(1) else domains).add(norm(m.group(2)))
                continue
            if line.startswith(('||', '@@', '|')) or any(c in line for c in '^$*'):
                skipped += 1; continue    # other adblock syntax (regex, wildcards, cosmetic) -> skip
            parts = line.split()
            if parts[0] in ('0.0.0.0','127.0.0.1','::1','::'):
                entries = parts[1:]
            else:
                entries = parts if len(parts) == 1 else []
            for d in entries:
                d = norm(d)
                if '.' in d and ' ' not in d:
                    domains.add(d)
    if allow:
        before = len(domains); domains -= allow
        print(f'白名单           : 移除 {before - len(domains):,} 个（{len(allow):,} 条 @@ 规则）', file=sys.stderr)
    if skipped:
        print(f'跳过的规则       : {skipped:,}（DNS 哈希列表无法表达的 adblock 语法）', file=sys.stderr)

    hashes = sorted(fnv(d.encode()) for d in domains)
    collisions = len(hashes) - len(set(hashes))
    uniq = sorted(set(hashes))                       # one entry per distinct hash
    with open(out, 'wb') as f:
        for h in uniq:
            f.write(h.to_bytes(HASH_BYTES, 'little'))

    n, size = len(uniq), len(uniq) * HASH_BYTES
    print(f'源域名           : {len(domains):,}')
    print(f'哈希条目         : {n:,}  （{HASH_BYTES} 字节 / {HASH_BYTES*8} 位）')
    print(f'碰撞             : {collisions}  （共享同一哈希的域名 -> 被误拦截）')
    print(f'flash 体积       : {size:,} 字节（{size/1024/1024:.2f} MB）-> {out}')
    print(f'查询             : 每次查询约 {math.ceil(math.log2(max(n,2)))} 次读取')

if __name__ == '__main__':
    main()
