# Release Pipeline (GitHub Actions)

This repo now ships a Windows release automatically from GitHub Actions and produces the update feed consumed by the app.

## How the workflow works

- Trigger: tag push matching `v*` (preferred) or manual `workflow_dispatch`.
- Qt: installs Qt 6.7.2 + MinGW 13.1 via `install-qt-action`.
- Build: configures CMake with `APP_VERSION` taken from the tag (strip leading `v`) and `UPDATE_FEED_URL` pointing to `https://github.com/Younthing/script-box/releases/latest/download/update.json`.
- Package: reuses `scripts/package.ps1 -SkipBuild -Zip` to run `windeployqt`, copy assets/tools, and zip to `dist/ScriptToolbox.zip`.
- Feed: writes `dist/update.json` with `{version,url,notes}` and uploads both `ScriptToolbox.zip` and `update.json` to the release and as build artifacts.

## Releasing (no web UI needed)

1) Make sure the repository is on GitHub and Actions are enabled.
2) Choose a version (must be `vX.Y.Z` to match the updater expectation). Tag and push:

   ```bash
   git tag v0.1.1
   git push origin v0.1.1
   ```

3) Wait for the `release` workflow. It will create a GitHub Release with the zip and `update.json`.
4) The app checks updates from the feed URL `https://github.com/Younthing/script-box/releases/latest/download/update.json`. Override at runtime with env `SCRIPT_TOOLBOX_UPDATE_URL` if you host it elsewhere.

## Local packaging checklist

- Configure with your desired version/feed:

  ```powershell
  cmake -S . -B build -G "MinGW Makefiles" -DAPP_VERSION=0.1.1 -DUPDATE_FEED_URL=https://github.com/Younthing/script-box/releases/latest/download/update.json -D CMAKE_PREFIX_PATH=D:/app/qt/6.10.1/mingw_64
  cmake --build build --config Release
  pwsh -File scripts/package.ps1 -QtDir D:/app/qt/6.10.1/mingw_64 -BuildDir build -OutputDir dist/ScriptToolbox -Zip -SkipBuild
  ```

- The resulting zip is `dist/ScriptToolbox.zip`; the feed JSON can be generated manually with the same `{version,url,notes}` shape used in CI.

## If GitHub Actions is unavailable (manual release)

1) Build release locally (example):
   ```powershell
   cmake -S . -B build -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE=Release -DAPP_VERSION=0.0.1 -DUPDATE_FEED_URL=https://github.com/Younthing/script-box/releases/latest/download/update.json -D CMAKE_PREFIX_PATH=D:/app/qt/6.10.1/mingw_64
   cmake --build build --config Release
   ```
2) Package:
   ```powershell
   pwsh -File scripts/package.ps1 -QtDir D:/app/qt/6.10.1/mingw_64 -BuildDir build -OutputDir dist/ScriptToolbox -Zip -SkipBuild
   ```
3) Generate update feed (set the final download URL you will publish):
   ```powershell
   pwsh -File scripts/make-update-feed.ps1 -Version 0.0.1 -ZipUrl https://github.com/Younthing/script-box/releases/download/v0.0.1/ScriptToolbox.zip -Notes "Manual release 0.0.1"
   ```
4) Publish to GitHub Release using GitHub CLI (no web UI):
   ```powershell
   gh auth login   # if not already authenticated
   gh release create v0.0.1 dist/ScriptToolbox.zip dist/update.json --title "Script Toolbox v0.0.1" --notes "Manual release 0.0.1"
   ```
   If you prefer another tool, upload `dist/ScriptToolbox.zip` and `dist/update.json` to the `v0.0.1` release assets via any HTTPS client.
