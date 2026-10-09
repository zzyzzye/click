#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
tag="${1:?请提供版本标签}"
python3 scripts/validate-release.py "$tag"
version="${tag#v}"
assets=(
  "ClickFlow-${version}-win64-setup.exe"
  "ClickFlow-${version}-win-arm64-setup.exe"
  "ClickFlow-${version}-macos-arm64.dmg"
  "ClickFlow-${version}-linux-x64.deb"
  "ClickFlow-${version}-linux-arm64.deb"
)
files=()
for asset in "${assets[@]}"; do
  for name in "$asset" "$asset.sha256"; do
    if [[ ! -s "release-assets/$name" ]]; then
      printf '缺少发布产物：%s\n' "$name" >&2
      exit 1
    fi
    files+=("release-assets/$name")
  done
  (cd release-assets && sha256sum --check "$asset.sha256")
done

# 重跑只允许更新草稿，避免覆盖已正式发布的安装包。
existing="$(gh api "repos/{owner}/{repo}/releases" --paginate --jq ".[] | select(.tag_name == \"$tag\") | .draft")"
if [[ "$existing" == "false" ]]; then
  printf '该版本已经正式发布，不能自动覆盖：%s\n' "$tag" >&2
  exit 1
elif [[ "$existing" == "true" ]]; then
  gh release edit "$tag" --title "ClickFlow $tag" --notes-file "docs/releases/$tag.md"
  gh release upload "$tag" "${files[@]}" --clobber
else
  gh release create "$tag" "${files[@]}" --draft --verify-tag \
    --title "ClickFlow $tag" --notes-file "docs/releases/$tag.md"
fi
printf '安装包已上传到草稿 Release：%s\n' "$tag"
