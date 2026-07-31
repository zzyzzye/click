# ClickFlow Public README Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite `README.md` as a public project landing page for ClickFlow users, contributors, and release maintainers.

**Architecture:** Keep all public project orientation in one README and organize it from product discovery to usage, safety, development, packaging, and release maintenance. Link to repository-native workflows and scripts instead of describing one developer's machine.

**Tech Stack:** Markdown, Qt 6.8.3, CMake 3.24+, C++20, GitHub Actions, Inno Setup.

## Global Constraints

- Do not hardcode the current product version in the project introduction.
- Use `<version>` or a shell variable in release examples.
- Do not describe a specific user's restricted computer.
- Do not claim automatic updates, code signing, macOS macro recording, or a license that does not exist.
- Preserve the documented macro privacy warning and Windows privilege boundary.
- Do not modify or stage the user's `CMakeLists.txt` and `tests/AppIdentityTests.cpp` changes.

---

### Task 1: Rewrite and verify the public README

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: repository facts from `CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/windows-package.yml`, `scripts/package-windows.ps1`, and the current feature implementation.
- Produces: a public README with sections `项目简介`, `主要功能`, `平台支持`, `下载安装`, `键鼠宏`, `安全与隐私`, `从源码构建`, `Windows 打包`, `发布流程`, and `平台限制`.

- [ ] **Step 1: Replace the current README structure**

Use this exact heading order:

```markdown
# ClickFlow
## 主要功能
## 平台支持
## 下载安装
## Windows 键鼠宏
## 安全与隐私
## 从源码构建
### Windows
### macOS
## Windows 打包
### 便携目录
### 安装包
## 维护者发布流程
## 平台限制
```

The introduction links to `https://github.com/zzyzzye/click/releases` and identifies Qt 6 Widgets/C++20 without embedding `0.4.0`.

- [ ] **Step 2: Make build and packaging commands reproducible**

Use Visual Studio 2022 and Qt `6.8.3 win64_msvc2022_64` as the canonical Windows example. Keep user-local Qt paths in `CMakeUserPresets.json`. Describe the `Windows 安装包` workflow as manually triggered from a `v<version>` tag and name its artifact `ClickFlow-Windows-x64`.

- [ ] **Step 3: Make release instructions version-neutral**

Use this command shape:

```powershell
$version = '<version>'
git tag -a "v$version" -m "release: 发布v$version"
git push origin "v$version"
```

Require the CMake version, tag, selected workflow ref, installer filename, and Release tag to match. State that the installer is unsigned.

- [ ] **Step 4: Review rendered Markdown and repository facts**

Run:

```powershell
rg -n "^#|0\.3\.0|受限开发电脑|你的电脑|QtClicker\.exe|自动发布" README.md
git diff --check -- README.md
git diff -- README.md
```

Expected: headings follow the approved order; obsolete/private wording is absent; Markdown has no whitespace errors; only documented repository capabilities are present.

- [ ] **Step 5: Commit only README**

```powershell
git add README.md
git commit -m "docs: 重构公共项目README"
```

Leave `CMakeLists.txt` and `tests/AppIdentityTests.cpp` unstaged for the user's release-version commit.
