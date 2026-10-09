#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
case "$(uname -m)" in
  x86_64) arch=x64; deb_arch=amd64 ;;
  aarch64) arch=arm64; deb_arch=arm64 ;;
  *) printf '%s\n' '错误：Linux 仅支持 x64 和 ARM64。' >&2; exit 1 ;;
esac
build_dir="build/linux-$arch-release"
output_dir="dist/linux-$arch"
cmake -S . -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCPACK_DEBIAN_PACKAGE_ARCHITECTURE="$deb_arch"
cmake --build "$build_dir" --parallel 4
# 图形测试采用无界面模式，平台输入测试单独使用虚拟 X11 显示。
QT_QPA_PLATFORM=offscreen xvfb-run -a ctest --test-dir "$build_dir" --output-on-failure
mkdir -p "$output_dir"
cpack --config "$build_dir/CPackConfig.cmake" -G DEB -B "$output_dir"
for artifact in "$output_dir"/*.deb; do
  actual_arch=$(dpkg-deb -f "$artifact" Architecture)
  [[ "$actual_arch" == "$deb_arch" ]] || { printf '%s\n' '错误：DEB 架构不匹配。' >&2; exit 1; }
  (cd "$output_dir" && sha256sum "$(basename "$artifact")" > "$(basename "$artifact").sha256")
  printf '%s\n' "已生成 Linux 安装包：$artifact"
done
