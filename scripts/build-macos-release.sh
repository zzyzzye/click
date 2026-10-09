#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
  printf '%s\n' '错误：macOS 安装包需要在 Apple Silicon 上构建。' >&2
  exit 1
fi
build_dir=build/macos-arm64-release
output_dir=dist/macos-arm64
qt_prefix=${QT_ROOT_DIR:-$(qtpaths --query QT_INSTALL_PREFIX)}
cmake -S . -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  "-DCMAKE_PREFIX_PATH=$qt_prefix"
cmake --build "$build_dir" --parallel 3
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build_dir" --output-on-failure
version=$(awk '/^project\(ClickFlow VERSION / {print $3}' CMakeLists.txt)
mkdir -p "$output_dir"
stage=$(mktemp -d "$output_dir/stage.XXXXXX")
trap 'rm -rf "$stage"' EXIT
ditto "$build_dir/ClickFlow.app" "$stage/ClickFlow.app"
macdeployqt "$stage/ClickFlow.app" -always-overwrite
if ! lipo -archs "$stage/ClickFlow.app/Contents/MacOS/ClickFlow" | grep -qw arm64; then
  printf '%s\n' '错误：应用不是 ARM64 架构。' >&2
  exit 1
fi
# 使用本地临时签名保证 Apple Silicon 可加载，不代表开发者签名或公证。
codesign --force --deep --sign - "$stage/ClickFlow.app"
codesign --verify --deep --strict "$stage/ClickFlow.app"
ln -s /Applications "$stage/Applications"
artifact="$output_dir/ClickFlow-$version-macos-arm64.dmg"
hdiutil create -volname ClickFlow -srcfolder "$stage" -format UDZO -ov "$artifact"
(cd "$output_dir" && shasum -a 256 "$(basename "$artifact")" > "$(basename "$artifact").sha256")
printf '%s\n' "已生成 macOS 安装包：$artifact"
