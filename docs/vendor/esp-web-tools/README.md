# 中文网页烧录组件

来源：[ESP Web Tools 10.1.1](https://github.com/esphome/esp-web-tools/tree/10.1.1)，使用 npm 发布包中的 `dist/web/*.js`，许可证见同目录 `LICENSE`。保留上游打包的依赖及其许可证注释。

本目录修改了串口选择帮助、安装确认、擦除提示、安装进度、配网和日志操作的中文文案。底层烧录协议、串口操作、分区地址和固件文件未改动。系统串口选择窗口由浏览器提供，其语言取决于浏览器设置；底层异常的原始技术详情和设备串口日志保留原文，便于排障。

页面通过相对路径加载本地组件，不再依赖 unpkg。修改翻译后，在项目根目录重新生成：

```powershell
python tools/localize_web_installer.py
```

翻译维护在 `tools/web_installer_zh.json`。生成器固定版本并校验下载包 SHA-256；任何翻译无法匹配时都会停止，不覆盖现有组件。组件文件保留上游文件名，以维持分块导入关系。

网页修改只需刷新或重新发布 `docs`，无需重新烧录开发板。

本地使用时不能直接双击 HTML。在项目目录运行 `python -m http.server 8000 --bind 127.0.0.1 --directory docs`，再打开 <http://127.0.0.1:8000/>。结束后按 `Ctrl+C` 关闭服务。
