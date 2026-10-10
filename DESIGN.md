---
version: alpha
name: ClickFlow
description: 以操作节奏为中心的浅色桌面自动化工具
colors:
  primary: "#0067C0"
  text: "#1B1B1B"
  secondary: "#616161"
  disabled: "#9E9E9E"
  danger: "#C42B1C"
  background: "#F5F7FB"
  surface: "#FFFFFF"
  sidebar: "#15243B"
  sidebar-text: "#CAD5E5"
  sidebar-accent: "#69D2FF"
typography:
  sans:
    fontFamily: 'Segoe UI Variable Text, Segoe UI, PingFang SC, Microsoft YaHei UI, Noto Sans CJK SC'
rounded:
  control: "8px"
  card: "14px"
spacing:
  page-gap: "16px"
  card-horizontal: "18px"
  card-vertical: "16px"
components:
  input:
    height: "32px"
  page-title:
    fontSize: "26px"
  card-title:
    fontSize: "15px"
---

# ClickFlow 设计约定

## 定位与依据

面向使用鼠标和键盘的桌面用户，主要任务是配置重复操作、启动并随时停止。界面使用简体中文，以 `docs/superpowers/specs/2026-08-31-fluent-light-redesign-design.md` 的浅色内容区为基础。2026-10-10 用户要求加强视觉美化、接入品牌 logo 和动效，本次更新采用深蓝品牌侧栏与浅色操作区，不引入整套深色主题或营销式大图。

视觉识别来自既有蓝色光环光标 logo、深蓝侧栏、浅青导航标记与始终可见的开始/停止操作。侧栏和关于页复用同一个 logo 资源，导航使用一致的线性图标。开始/停止按钮切换颜色后立即刷新样式；表单保持克制。

## 唯一实现来源

采用现有运行时代码为主的模型：`src/app/UiStyle.h` 的 ThemeTokens 与 `src/app/UiStyle.cpp` 的 QSS 是颜色和控件样式的来源，本文件记录其用途。导航绘制复用 ThemeTokens；页面标题和卡片布局由 MainWindow 的统一页面装配管理。调整系统样式时同步更新代码与本文。

## 字体与布局

正文 13px，页面标题 26px、600 字重，卡片标题 15px、600 字重；平台字体回退覆盖 macOS、Windows 和 Linux 的中文。说明文字使用次要文本颜色；错误保留文字说明，不仅依赖颜色。

侧栏宽 200px，内容区横向留白 20px，卡片内边距横向 18px、纵向 16px。最小窗口 820×560，默认 960×680。四个页面统一标题、说明、卡片结构；底部操作区和顶部状态区保持可见。长文字换行，较矮窗口由页面内部垂直滚动承载内容，不压缩表单或产生横向滚动。macOS 和 Linux 使用系统标题栏，不额外绘制 Windows 窗口控制按钮。

连点表单只展示当前模式相关的键盘、鼠标、固定坐标和有限次数字段，隐藏不清空已填写的值。预设为空时展示下一步提示，避免大片空白列表。

## 控件与滚动

使用 Qt 原生表单语义、焦点及下拉弹层；保留键盘导航与编辑。控件高度 32px、圆角 8px，卡片圆角 14px，以边框和表面区分层级，不叠加阴影。基础箭头、勾选图标使用三倍分辨率 PNG，避免本机缺少 Qt SVG 插件时不可见；保留 SVG 源资源。

悬停在未展开的下拉框、数字框及其文本区域时，滚轮只滚动所在页面，不更改参数；有焦点也遵循此规则。展开的下拉列表及预设列表保留各自的滚动。触控板像素位移直接响应；离散鼠标滚轮采用 160ms 缓出，快速同向输入累计目标位移，反向立即响应。边界不回弹。

## 动效

页面切换使用 180ms 缓出淡入，立即切换内容，不等待动画才能操作。快速切页停止上一次动画；动画完成后关闭图像效果，避免持续离屏合成。侧栏提供本次会话的“减少动态效果”开关，可立即取消淡入及滚轮缓动；Qt 平台样式关闭控件动画时也不启用淡入。没有持续循环、闪烁或影响紧急停止响应的动画。

## 验证

MainWindowTests 覆盖滚轮转交、不误改参数、快速输入、边界，以及四页在最小和默认窗口尺寸的横向适配。测试使用假输入服务和 offscreen 平台，生成临时预览图片供视觉检查。Windows、Linux 的原生弹层和真实触控板手感仍需对应平台实机验证。
