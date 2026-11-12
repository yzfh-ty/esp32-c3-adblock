# esp32-c3-adblock

本仓库是 **fork**，原作者/上游项目为 [M-Abozaid/esp32-c3-adblock](https://github.com/M-Abozaid/esp32-c3-adblock)。

一个**类 Pi-hole 的 DNS 广告拦截器**，运行在 **2 美元的 ESP32-C3** 上——*无需 PSRAM*。
同样支持 **ESP8266**（NodeMCU / D1 mini，4 MB flash）——见 [ESP8266 说明](#esp8266-notes)。

> 📰 曾获 [Tom's Hardware](https://www.tomshardware.com/networking/clever-hacker-fits-537-000-domains-in-a-tiny-usd5-esp32-ad-blocking-dongle-firmware-uses-only-around-50kb-of-ram-and-can-answer-blocked-lookups-in-10-milliseconds)、[XDA Developers](https://www.xda-developers.com/this-tiny-esp32-powered-gadget-blocks-537000-domains-only-uses-50kb-of-ram/) 和 [Korben](https://korben.info/en/half-million-ad-blocking-domains-50kb-ram-esp32.html) 报道。

这里的关键技巧是：你不需要把拦截列表放进 RAM。把域名以**排序后的 40 位哈希**存储到 flash 中，然后用二分查找。140,000+ 域名只占约 0.7 MB 的 flash，匹配耗时约 10 ms，且仅用**约 50 KB RAM**。

```
query in ──▶ extract domain ──▶ FNV-1a hash (+ parent suffixes)
         ──▶ binary-search the flash hash table
              ├─ hit  ──▶ answer 0.0.0.0   (sinkholed)
              └─ miss ──▶ forward to upstream resolver, relay the reply
```

## 为什么值得关注

大多数 ESP32 DNS 拦截器把拦截列表（域名字符串）加载到 RAM 中，因此需要 PSRAM。本项目改为在 flash 中存储固定的 **5 字节（40 位）哈希**：

| | 字符串进 RAM 方案 | 本方案（哈希进 flash） |
|---|---|---|
| 硬件 | ESP32 + PSRAM（约 $8） | ESP32-C3，无 PSRAM（约 $2） |
| 14.1 万域名 | 约 2.5 MB RAM | **0.67 MB flash** |
| 占用的 RAM | 几乎全部 | **约 50 KB** |
| 查询 | 字符串比较 | 约 18 次 flash 读取（约 10 ms，含 WiFi 往返） |
| 碰撞 | 无 | 14.1 万时 0 个（53.7 万时 1 个） |

**为什么是 40 位？** 这是这个 flash 容量下的最佳平衡点。碰撞遵循生日界限——14.1 万域名时约 0 个，53.7 万时约 1 个（即某个倒霉域名会被误拦截）。降到 32 位虽能省 20% 的 flash，但 25 万条时会产生约 7 次碰撞；升到 64 位每个域名多占 3 字节，去解决一个根本不存在的问题。

同样的技巧也适用于更大的芯片——它并非 C3 的权宜之计。在 16 MB 的 ESP32-S3 上，这些哈希可容纳 **约 270 万域名**，而 8 MB PSRAM 只装得下约 46.6 万个字符串域名。哈希进 flash 几乎在所有场合都优于字符串进 PSRAM；C3 只是把这个结论体现得最为彻底。

## 硬件

- 任意 **ESP32-C3** 开发板（已在 C3 SuperMini 上测试），4 MB flash，**无需 PSRAM**
- 经典 **ESP32**（DevKit / WROOM，4 MB）同样可构建：`pio run -e esp32dev -t upload` 和 `pio run -e esp32dev -t uploadfs`（已在真实硬件上验证，见 #40；C3 是主要测试目标）。其温度读数未经校准，仅供参考。
- **ESP8266**（NodeMCU / D1 mini / 任意 4 MB flash 的 ESP8266EX）同样可构建：`pio run -e esp8266 -t upload`——不同点见 [ESP8266 说明](#esp8266-notes)
- 使用**稳定的 USB 电源**（手机充电器或路由器的 USB 口）供电。廉价/松动的 USB-C→A 转接头可能在 WiFi 发射时造成电压跌落（brownout）。
- 用 **USB-A → USB-C 转接头**可直接插入大多数路由器背面的空闲 USB 口——无需独立电源，无需额外盒子。

### ESP8266 说明

ESP8266 同样运行拦截器——它本就没有 PSRAM，拦截列表无论怎样都以 40 位哈希形式存在 flash 中。它是最慢、最局促的目标，但也是你能插进路由器的最便宜的板子。

```bash
pio run -e esp8266 -t upload
pio run -e esp8266 -t uploadfs
```

在 ESP8266 上的不同点（C3 和经典 ESP32 构建不受影响）：

| | ESP32-C3 | ESP8266 |
|---|---|---|
| 占用 flash | 约 1.24 MB | 1,044,464 字节应用分区中的约 0.50 MB |
| 静态 RAM | 320 KB 中的 78 KB | 80 KB 中的 44 KB |
| Flash 索引（`INDEX_ENTRIES`） | 4096 | 1024 |
| 哈希缓存（`CACHE_SIZE`） | 256 | 64 |
| 追踪的客户端 / 自定义域名 | 96 / 200 | 32 / 50 |
| 客户端识别 | IP + MAC（lwIP ARP 表） | 仅 IP（未暴露 ARP 表） |
| 温度读取 | 内部传感器 | 0 °C（无传感器） |
| 文件系统 | 1.3 MB LittleFS | 1 MB LittleFS |

较小的索引依然能无遗漏地解析 112,556 域名的默认列表：最宽的桶为 112 个哈希，完全落在 256 哈希的扫描窗口内，因此每个已存储的哈希都可被命中。如果你加载更大的列表，请提高 `INDEX_ENTRIES`，并保持 `MAX_RANGE >= ceil(domains / INDEX_ENTRIES)`——`inFlash()` 会把扫描的桶钳制到 `MAX_RANGE`，因此过小的值会静默地漏掉哈希。

两个垫片让它能工作：`src/prefs_shim.h` 把 ESP32 的 NVS `Preferences` API 用 ESP8266 的 EEPROM 实现，`getMac()` 被空实现替代（ESP8266 不暴露 ARP 表）。固件 OTA 把镜像暂存在应用与文件系统之间的空闲草图空间，HTTPS 拦截列表获取使用 BearSSL（`setInsecure()`、`setBufferSizes(8192, 1024)`）而非 mbedTLS。

### 外壳

为 C3 SuperMini 准备了可打印外壳：[`hardware/esp32-c3-supermini-enclosure.stl`](hardware/esp32-c3-supermini-enclosure.stl)

打印注意事项：
- 无需支撑；0.2 mm 层高、约 15% 填充即可。
- **保持天线端空旷。**C3 的 PCB 天线是 USB-C 口对侧短边上的锯齿状走线——不要把它埋进实心塑料里，也不要在它附近放金属，否则你的 RSSI（信号强度）会受损。
- 保留通风口打开：板子待机温度约 45–55 °C。

C3 外壳**不**适配 ESP8266 开发板（它是 44 × 25 mm，按 SuperMini 尺寸设计）。对于 NodeMCU / D1 mini 这类板子，请使用参数化外壳：[`hardware/esp8266-enclosure.py`](hardware/esp8266-enclosure.py) 直接生成 STL（无需 CAD 工具），[`hardware/esp8266-enclosure.scad`](hardware/esp8266-enclosure.scad) 是给 OpenSCAD 用的相同几何体。已为 30 × 58 mm 的 NodeMCU 预生成：[`hardware/esp8266-nodemcu-enclosure.stl`](hardware/esp8266-nodemcu-enclosure.stl)

先测量你的板子——两排排针之间的宽度，以及从 USB 端到天线端的长度——然后重新生成：

```
python3 hardware/esp8266-enclosure.py --board-w 30 --board-l 58 --out case.stl
```

它是一个托盘式设计：板子从顶部放入，侧边凸筋卡住排针条，端壁为 USB 插头留出槽位，天线端保持开放。打印注意事项与上文相同。

## 构建与烧录（PlatformIO）

网页烧录入口在 `docs/index.html`。烧录组件已固定为本地中文版本，串口帮助、安装确认、擦除提示、进度和操作按钮均为中文；维护方式见 [`docs/vendor/esp-web-tools/README.md`](docs/vendor/esp-web-tools/README.md)。更新网页后刷新即可，无需重新烧录设备。浏览器自己的串口选择窗口使用浏览器设置的语言。

网页已内置 **ESP32-C3 / 4 MB Flash 中文默认固件**及 138,121 条域名的拦截列表。点击“烧录默认固件”，选择串口，再点击“安装 C3 AdBlocker”并确认即可，无需编译或手动选择 `.bin` 文件。默认镜像使用 `src/secrets.example.h` 构建，不含个人 WiFi 凭据，烧录后通过热点配网。管理用户名为 `admin`，管理密码为 `CHANGE_ME_WEB_PASSWORD`，网络 OTA 密码为 `CHANGE_ME_OTA_PASSWORD`；这些是公开默认值，需要自定义密码时请按下方流程自行编译。

不能直接双击 HTML 文件（`file://`）进行烧录。在项目目录运行以下命令，然后用桌面版 Chrome / Edge 打开 <http://127.0.0.1:8000/>。使用期间保持终端打开，结束后按 `Ctrl+C` 关闭服务。

```powershell
python -m http.server 8000 --bind 127.0.0.1 --directory docs
```

只需一次 USB 烧录即可启动——之后**固件和拦截列表都能通过 WiFi 更新**（见下文）。

> ⚠️ 请使用**较新的 PlatformIO**——VSCode PlatformIO 扩展自带的核心，或在虚拟环境中 `pip install -U platformio`。发行版/apt 的 `platformio` 包（例如 4.3.4）太旧，会报 `AttributeError: ... 'resultcallback'`（issue #4）。平台版本已锁定在 `platformio.ini`（更新版本构建的 C3 镜像对 OTA 分区来说过大了）。

```bash
# 1. copy the secrets template (gitignored, stays local) and edit it:
#    - WIFI_SSID / WIFI_PASS are optional — leave the placeholders and use the
#      on-device setup portal instead (below).
#    - WEB_USER / WEB_PASS / OTA_PASS are NOT optional: they gate the dashboard's
#      state-changing endpoints (/ban, /addblock, /upload, /update, /setupdate,
#      /forgetwifi) and network OTA. Pick real values — these used to be wide
#      open to anyone on the LAN.
cp src/secrets.example.h src/secrets.h
#    then edit src/secrets.h

# 2. 构建拦截列表哈希表（默认 = StevenBlack base + Hagezi Light，
#    约 10 万条，WhatsApp/社交应用不受影响）
python3 tools/build_blocklist.py data/blocklist.bin

# 3. 烧录固件 + 拦截列表文件系统（唯一的一次 USB 烧录）
pio run -t upload
pio run -t uploadfs          # 经典 ESP32：两条命令都要加 -e esp32dev

# 4. 观察启动，记下 IP / 打开仪表盘
pio device monitor          # -> http://c3adblock.local
```

### 使用你自己的拦截列表

`build_blocklist.py OUT.bin [SOURCE ...]` 接受 URL 和本地文件的任意组合，支持以下格式：

- **hosts 文件** — `0.0.0.0 ads.example.com tracker.example.com`（该行所有域名都会被包含）
- **纯域名列表** — 每行一个域名
- **AdGuard / Adblock 基础规则** — `||ads.example.com^` 拦截，`@@||ok.example.com^` 移除某个域名（例如用来镜像一个 AdGuard Home 白名单）

一个被拦截的域名也会拦截它的所有子域名。DNS 哈希列表无法表达的规则（正则、通配符、`$` 修饰符、`##` 美化规则）会被跳过并计数。`@@` 规则只解除那一个精确条目的拦截——它无法从被拦截的父域名中抠出某个子域名。如果某个源无法下载，构建会停止，而不会静默地生成一个更小的列表（可用 `--allow-missing` 覆盖）。

### WiFi 设置（无需重新烧录）

如果它无法连接（或者你从未设置 `secrets.h`），它会启动一个带强制门户的开放接入点
**`C3-AdBlock-XXXX`**——用手机连上它，选择你的网络，输入password，完成。之后想换网络：在仪表盘点 **忘记 WiFi**，或在开机时按住 **BOOT** 键，设置门户就会重新出现。（`/forgetwifi` 现在需要鉴权，所以它不再是一个能直接访问的裸 URL——见下方「安全」）

部分 C3 SuperMini 板子的天线匹配不佳，满发射功率下会异常（#50）。设置 AP 固定以 8.5 dBm 运行，如果 C3 在满功率下无法加入你的 WiFi，它会以 8.5 dBm 再重试一次。要固定功率等级，可在 `build_flags` 中加入例如 `-DWIFI_TX_POWER=WIFI_POWER_8_5dBm` 构建。

## 空中（OTA）更新（不再需要 USB）

仪表盘 **http://c3adblock.local** 就能完成所有操作：

- **拦截列表** — 把新构建的 `blocklist.bin` 拖入 *拦截列表 → 上传*，或设置 *远程自动更新* 的 URL，设备会按计划拉取预生成的 `blocklist.bin`。GitHub Actions **每周一**自动重建一份新的默认列表并发布到稳定 URL，因此粘贴一次即可让设备自行保持最新：
  `https://github.com/M-Abozaid/esp32-c3-adblock/releases/download/blocklist/blocklist.bin`
- **固件** — 在 *固件 → OTA 更新* 上传 `.pio/build/c3/firmware.bin`；设备会校验并重启进入新镜像。或者从命令行通过 WiFi 推送：
  ```bash
  pio run -t upload --upload-port c3adblock.local --upload-protocol espota
  ```

**4 MB flash 的权衡：**固件 OTA 需要*两个*应用分区，这给拦截列表留下约 1.3 MB（**最多约 25 万域名**）。激进的 53.7 万 "ultimate" 列表只装得下单一应用分区表（无固件 OTA）。在 `partitions.csv` 中选择你的取舍。

## 安全

仪表盘的只读视图（`/`、`/stats.json`）保持开放，但每个改变状态的端点都要求 **HTTP 基本认证（Basic Auth）**（来自 `secrets.h` 的 `WEB_USER`/`WEB_PASS`）：

- `/ban`、`/addblock`、`/unblock`、`/forgetwifi`
- `/upload`、`/update`（拦截列表和固件 OTA）
- `/setupdate`、`/fetchnow`

网络 OTA（`ArduinoOTA`，例如 `pio run -t upload --upload-port c3adblock.local --upload-protocol espota`）需要同一个文件里的 `OTA_PASS`。

如果没有这些保护，任何能触达局域网内这台设备的人都能无需任何凭据就重新刷写任意固件或改写拦截列表——考虑到这台设备处于你网络上每条 DNS 查询的通路上，这一点值得注意。自定义的被拦截域名在渲染到仪表盘前也会先做 HTML 转义，封堵了一条存储型 XSS 通道——否则通过 `/addblock` 添加的包含标记的域名字符串会在查看者的浏览器中执行。

**这里的 Basic Auth 是局域网信任边界的管控手段，而不是加密。**所有内容都是 :80 端口上的明文 HTTP——这块芯片没有实际预算去运行 TLS 服务器。Basic Auth 凭据是 base64（非加密）并在每次已认证请求中发送；任何已经能嗅探你局域网流量的人（开放/访客 WiFi、ARP 欺骗）都能从线缆上读取它们。这加固了常见场景——你网络上的另一台设备不带任何凭据来打 API，或某个浏览器标签页对它的 CSRF——但无法抵御处于路径上的网络攻击者。

**CSRF 经由缓存的 Basic Auth：**浏览器会把缓存的 Basic Auth 凭据自动附加到对已认证源（origin）的*任意*后续请求——包括同一个浏览器之后访问的某个完全无关页面所触发的请求（例如 `<img src="http://c3adblock.local/forgetwifi">`，无需 JS）。这会让任何网页在你登录过一次仪表盘后，无论谁在局域网上，都能悄无声息地驱动这个 API。上面每个改变状态的端点现在也都要求一个自定义的 `X-Requested-With: c3-adblock` 请求头，普通的 `<img>`/自动提交的 `<form>` CSRF 无法附加它（只有同源的 `fetch()` 能附加，而这正是仪表盘自身 JS 的做法）——这就是为什么 `/forgetwifi` 不再是一个能直接访问的裸 URL；请改用仪表盘上的按钮。

**默认凭据：**如果 `secrets.h` 仍然保留 `secrets.example.h` 里的占位值 `CHANGE_ME_WEB_PASSWORD` / `CHANGE_ME_OTA_PASSWORD`，设备会带一个公开的「password」启动（它就在本仓库的示例文件里）。这种情况下固件会在串口记录一条警告，并在仪表盘上显示横幅——但它仍会启动运行，所以在你不完全信任的网络上使用前，请务必在 `secrets.h` 中设置真实值。

目前不在范围内：WiFi 设置门户的接入点（`C3-AdBlock-XXXX`）按设计仍是开放的（无加密）——它需要在不知道password的情况下就能连接。你在门户里输入的真实 WiFi password 的安全性，只相当于那段短暂的设置窗口期内本地无线电链路的安全性。

## 使用

把某个设备的 DNS 指向 C3 的 IP，或把它作为主 DNS 之后的**备用解析器**。

**上游解析器：**放行的查询默认前往 Quad9。你可以在仪表盘的 **上游 DNS** 下更改它（预设值、任意 IPv4，或「使用路由器的 DNS」）。如果这台设备是路由器分发的 Pi-hole/AdGuard 的*备用*，请保持公共解析器，因为转发到它恰好在需要备用时失败。（在中国，AliDNS `223.5.5.5` 比 Quad9 快得多；见 #47。）

**IPv6：**这个解析器通过 IPv4 应答。如果你的路由器还通告了一个 IPv6 DNS 服务器（RDNSS/DHCPv6），设备可能会使用它从而绕过拦截器，因此请在路由器上将其关闭或指向别处。

Test:

```bash
dig @<c3-ip> doubleclick.net   # -> 0.0.0.0  (blocked)
dig @<c3-ip> github.com        # -> real IP  (forwarded)
```

## 常见坑（血泪教训）

- **ModemManager**（Fedora/Ubuntu 默认开启）会占用 `/dev/ttyACM0` 并翻转 DTR/RTS，这会**重置 C3** 并阻塞串口。解决方法：
  ```bash
  sudo systemctl stop ModemManager
  echo 'ATTRS{idVendor}=="303a", ENV{ID_MM_DEVICE_IGNORE}="1"' | sudo tee /etc/udev/rules.d/99-esp-no-modemmanager.rules
  sudo udevadm control --reload-rules && sudo udevadm trigger
  ```
- C3 的 USB-Serial-JTAG 控制台在主机连接之前可能会吞掉早期的启动输出（`while(!Serial)` 有帮助）。
- DNS 客户端会添加一个 **EDNS OPT** 记录；被拦截的应答必须只包含问题 + 答案（ANCOUNT=1, NSCOUNT=ARCOUNT=0），否则会被视为畸形。

## 已完成 / 未来展望

- ✅ Web 仪表盘——每个客户端的拦截/放行计数、封禁客户端、添加自定义域名
- ✅ mDNS（`c3adblock.local`）用于发现
- ✅ OTA——通过 WiFi 更新固件 + 拦截列表，以及按计划的远程拦截列表拉取
- ✅ 强制门户 WiFi 配置（无硬编码凭据）
- ✅ 每周拦截列表发布 + 通过校验的更新（一次坏下载绝不会替换掉可用的列表）
- ✅ 可配置的上游解析器
- ⬜ 一键浏览器网页刷写工具（构建于 `docs/`，需要重新托管）
- ⬜ DNS 应答缓存 + 非阻塞转发（#40、#47）
- ⬜ 更小的 flash 格式（#3/#21）：3 字节后缀，每 MB 约多 40% 域名
- ⬜ 充当 DHCP 服务器（把自己作为 DNS 分发出去），实现真正的即插即用

## 致谢

灵感来自 [s60sc/ESP32_AdBlocker](https://github.com/s60sc/ESP32_AdBlocker)——「对被拦截的域名返回 0.0.0.0」的想法。这是一个独立实现的版本，专注于无 PSRAM 芯片上的哈希进 flash 优化。

## 许可证

MIT——见 [LICENSE](LICENSE)。
