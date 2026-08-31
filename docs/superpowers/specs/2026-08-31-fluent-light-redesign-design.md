# ClickFlow Fluent 浅色改造设计

日期：2026-08-31
状态：已获用户批准

## 背景与目标

ClickFlow（Qt Widgets 连点器，当前 0.5.0）已具备基础的 Win11 Mica 窗口外壳，但存在两个缺陷，且整体视觉未达 Fluent 标准：

- **缺陷 A**：主内容区渲染为纯黑色，Mica 云母效果未生效
- **缺陷 C**：窗口右边缘出现文字/控件渲染残留（溢出）
- **目标 B**：完整 Fluent 浅色改造 + 沉浸式自定义标题栏

已确认的关键决策：

| 决策点 | 结论 |
|---|---|
| 美化范围 | 完整 Fluent 改造（对标 Win11「设置」应用） |
| 标题栏 | 沉浸式自定义标题栏（自绘窗口控制按钮 + Snap Layouts） |
| 主题 | 仅浅色，不做深色；DWM 暗色模式固定关闭 |
| 架构路线 | 主题令牌 + 模板化 QSS（自研，无外部依赖） |

## 1. 前置缺陷修复

### 1.1 Mica 黑背景（缺陷 A）

现状：[WindowsWindowStyle::apply()](src/platform/windows/WindowsWindowStyle.cpp) 在窗口显示后设置 `DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_MAINWINDOW`，配合 `Qt::WA_TranslucentBackground` 让 Mica 透出。当前透明区域渲染为纯黑。

排查与修复（按嫌疑排序）：

1. **HWND 重建丢失 DWM 属性**（首要嫌疑）：`MainWindow::applyWindowOnTop()` 调用 `setWindowFlag(Qt::WindowStaysOnTopHint)` 会销毁并重建 HWND，已设置的圆角 / backdrop / 暗色模式属性全部丢失，且透明窗口在 backdrop 缺失时渲染为黑色。
   修复：在 `WindowsWindowStyle` 中提供统一的「句柄重建后重应用」入口，由 `MainWindow` 在 `showEvent`、`nativeEvent`（窗口创建/重建）及 `applyWindowOnTop()` 之后调用。
2. **静默失败**：`DwmSetWindowAttribute` 返回值从未检查。
   修复：检查 HRESULT，失败时记录日志并标记 backdrop 不可用。
3. **系统「透明效果」关闭**：此时 Windows 禁用 Mica。
   修复：读取 `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize\EnableTransparency`，关闭时回退不透明浅色背景（复用现有 `clickFlowStyleSheet(bool translucent)` 参数机制）。

### 1.2 右侧溢出（缺陷 C）

窗口右边缘有文字残片（疑似 "te" 字样）与滚动条渲染异常。排查 `QStackedWidget` 各页（重点 `HotkeySettingsPage`、`MacroRecordingPage`）的最小宽度约束与滚动区域 viewport，修复使内容不超出窗口边界。根因待排查后确认，修复方式限定为布局约束修正，不改交互逻辑。

## 2. 主题系统（浅色单套，令牌化）

### 2.1 ThemeTokens

在 `UiStyle` 模块中新增 `ThemeTokens` 结构体，集中定义 Fluent 浅色设计令牌：

- **表面层**：窗口基底（透明，透出 Mica）、侧边栏半透明白（约 `rgba(255,255,255,0.6)`）、卡片纯白、卡片描边
- **文本色阶**：主文本、次文本、禁用文本
- **控件**：背景、描边、悬停、按下、禁用、focus 描边
- **accent**：`#0067C0`（Win11 默认蓝），悬停/按下派生色
- **几何**：控件圆角 4px、卡片/弹层圆角 8px、控件标准高度 32px

### 2.2 模板化 QSS

现有 `clickFlowStyleSheet(bool translucent)` 全量重写为「令牌注入模板」：`UiStyle::styleSheet(const ThemeTokens&)` 生成整份样式表。单套浅色令牌即全局唯一实例，不做主题切换。

DWM 侧：`DWMWA_USE_IMMERSIVE_DARK_MODE` 固定为 `FALSE`，删除 `readAppsUseLightTheme()` 分支逻辑。

## 3. 沉浸式标题栏 CaptionBar

### 3.1 技术方案

采用 **WM_NCCALCSIZE 去除非客户区**（在 `nativeEvent` 中拦截），而非 `Qt::FramelessWindowHint`：

- 保留 DWM 原生圆角、窗口阴影、Snap 吸附与 Mica backdrop
- 保留原生边框拖拽调整大小
- 避免 `FramelessWindowHint` 丢阴影/丢圆角/任务栏动画异常等已知问题

### 3.2 CaptionBar 组件

新增 `src/app/widgets/CaptionBar.{h,cpp}`，置于主窗口顶部：

- 左侧：应用图标 + 窗口标题（跟随 `windowTitle`）
- 右侧：最小化 / 最大化-还原 / 关闭 三个自绘按钮，Fluent 样式（32px 高，悬停浅灰底，关闭按钮悬停 `#C42B1C` 白字）
- 拖拽区：空白区域拖动窗口；双击切换最大化/还原
- **Snap Layouts**：`WM_NCHITTEST` 命中最大化按钮时返回 `HTMAXBUTTON`，使 Win11 悬停 Snap 布局面板生效
- DPI 缩放：按钮尺寸与图标按 devicePixelRatio 适配；`WM_DPICHANGED` 时刷新
- 窗口状态同步：最大化时切换最大化/还原图标，标题栏不再承担双击

### 3.3 与 MainWindow 的装配

`MainWindow::buildUi()` 中将 `CaptionBar` 作为中央区域顶行；原系统标题栏经由 WM_NCCALCSIZE 方案隐藏。`nativeEvent` 处理集中在 `MainWindow`（或抽为 `FramelessWindowHelper`，视实现复杂度决定，优先集中在 MainWindow 内私有方法）。

## 4. 视觉改造要点

- **侧边栏**：移除不透明 `#e9ecf1` 底与右边框线，改为 Mica 上的半透明层；导航项选中态由「整块蓝填充」改为 Fluent 标准「浅灰底 + 左侧 3px accent 指示条」，悬停浅灰
- **卡片**（settingsCard / statusStrip / actionBar）：纯白底、8px 圆角、1px 浅描边、无阴影
- **控件**：QPushButton / QComboBox / QSpinBox / QKeySequenceEdit 统一 32px 高（主操作按钮 36px）、4px 圆角、Fluent focus 描边（accent 色 1px + 内描边）；禁用态色阶按令牌
- **滚动条**：Fluent 细滚动条（默认约 6px 半透明，悬停展开加深），覆盖 SmoothScrollArea 及各页滚动区
- **主按钮**：accent 蓝 `#0067C0`，运行中红色保留（色值按 Fluent 规范微调）
- **字体**：Segoe UI（存在则用 Segoe UI Variable Text），标题/正文/辅助三级字号
- 其余控件（QCheckBox、QSlider、QToolTip、QMenu、对话框内控件）按令牌补齐 Fluent 风格 QSS，覆盖四个页面实际用到的控件类型

## 5. 改动清单

| 文件 | 改动 |
|---|---|
| `src/app/widgets/CaptionBar.{h,cpp}` | 新增：沉浸式标题栏组件 |
| `src/app/UiStyle.{h,cpp}` | 重写：ThemeTokens + 模板化 Fluent 浅色 QSS |
| `src/app/MainWindow.{h,cpp}` | 装配 CaptionBar；nativeEvent（WM_NCCALCSIZE / WM_NCHITTEST / WM_DPICHANGED）；句柄重建后重应用窗口样式 |
| `src/platform/windows/WindowsWindowStyle.{h,cpp}` | DwmSetWindowAttribute 错误检查与日志；EnableTransparency 检测；句柄重建重应用入口；移除暗色分支 |
| `src/app/widgets/NavigationSidebar.{h,cpp}` | 选中指示条样式（绘制或 QSS 配合） |
| 各 Page | 仅必要的间距/尺寸微调，不改交互逻辑 |

## 6. 错误处理与兜底

- Mica 不可用（Win10、透明效果关闭、DWM 调用失败）→ 不透明浅色兜底，界面功能与观感保持完整
- 自定义标题栏按钮失效兜底：系统菜单（Alt+Space）与任务栏右键仍可用（WM_NCCALCSIZE 方案天然保留）

## 7. 验证

手动验证清单：

1. Mica 云母正常透出，内容区不再黑色
2. 右边缘无渲染残留，四个页面均无横向溢出
3. 标题栏：拖拽移动、双击最大化/还原、三按钮功能、最大化按钮悬停出现 Snap Layouts
4. 切换窗口置顶后 Mica 与标题栏表现不退化（HWND 重建路径）
5. 最大化 → 还原、贴边 Snap 后还原，布局正常
6. 高 DPI / 多显示器 DPI 切换无尺寸错乱
7. 关闭系统「透明效果」后回退不透明浅色，界面仍完整可用
8. 现有自动化测试回归通过

## 8. 非目标（YAGNI）

- 深色主题（含跟随系统切换、主题监听器）
- 应用内主题设置项
- 亚克力（Acrylic）backdrop、Mica Alt
- 交互逻辑/功能变更
