# Script Toolbox (Qt/Win)

基于 Qt 的桌面容器，扫描 `tools/` 下的脚本/CLI 工具，自动生成表单、管理运行环境，并支持自更新。

## 依赖

- Windows + PowerShell 5.1+
- CMake ≥ 3.21
- Qt 6 (示例：6.10.1 mingw_64) + MinGW 13.1
- 可选：GitHub CLI (`gh`) 用于创建 Release

## 构建 + 打包（最简流程）

```powershell
# 配置 + 编译
cmake -S . -B build -G "MinGW Makefiles" `
  -D CMAKE_BUILD_TYPE=Release `
  -D CMAKE_PREFIX_PATH=D:/app/qt/6.10.1/mingw_64 `
  -D APP_VERSION=0.0.2 `
  -D UPDATE_FEED_URL=https://github.com/Younthing/script-box/releases/latest/download/update.json
cmake --build build --config Release

# 打包（windeployqt + assets/tools → dist/ScriptToolbox.zip）
pwsh -File scripts/package.ps1 -QtDir D:/app/qt/6.10.1/mingw_64 -BuildDir build -OutputDir dist/ScriptToolbox -Zip -SkipBuild

# 运行
build\simpleqt.exe
```

## 发布（单脚本完成）

```powershell
# 构建/打包/生成 update.json，可选 tag/push/release
pwsh -File scripts/release.ps1 `
  -AppVersion 0.0.2 `
  -QtDir D:/app/qt/6.10.1/mingw_64 `
  -FeedUrl https://github.com/Younthing/script-box/releases/latest/download/update.json `
  -Tag -Push -Release   # 去掉任意开关可只生成本地产物
```

产物：`dist/ScriptToolbox.zip`、`dist/update.json`，可直接作为 Release 附件或自托管。

## 更新机制

- 默认更新源：`https://github.com/Younthing/script-box/releases/latest/download/update.json`
- 运行时可用环境变量覆盖：`SCRIPT_TOOLBOX_UPDATE_URL=http://.../update.json`
- 更新流程：下载 zip → 写临时目录 → 生成/执行 `apply_update.ps1` → 替换文件 → 启动新版本；日志写入临时目录 `update.log`。

## 目录

- `src/`：核心逻辑与 UI
- `tools/`：示例工具
- `scripts/`：打包、发布、更新源生成
- `docs/release.md`：发布说明

## License

- Project code: MIT (see `LICENSE`).
- Qt runtime: LGPLv3。再分发时请保持动态链接（windeployqt 已按此打包）、保留 Qt 版权/许可说明，提供 Qt 源码获取方式（<https://download.qt.io/archive/qt/），并允许替换> Qt 动态库。
