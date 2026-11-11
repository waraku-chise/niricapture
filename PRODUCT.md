# Product

<!-- impeccable:product-schema 1 -->

## Platform

linux

## Users

- **第一用户（据仓库与实机环境推断，口径待确认）：仓库作者**。Niri + DMS 的 Arch Linux 日常使用者；截图 → 标注 → 复制/保存是高频动作，Niri 键位、托盘、就地编辑器和独立编辑器是每天用的入口。
- **次要用户：Niri 用户**（公开分发：GitHub `waraku-chise/niricapture`、`packaging/PKGBUILD`、Release 里的 deb）。他们想要一个按 Niri 原生方式工作、不依赖 Quickshell/合成器插件的截图工具；DMS 可有可无。
- **典型场景**：单输出 1920×1080（开发机）上的区域/整屏截图；对完全被遮挡、位于非当前工作区或带透明度的窗口做窗口截图；需要快速标注后直接复制或存盘；OCR 文本提取。
- **明确未定**：1.0 的优先级是“作者日常效率”还是“新用户首次上手/安装体验”，尚未确认。

## Product Purpose

NiriCapture 是面向 Niri（Wayland）的原生截图与标注工具：区域/整屏截图先冻结所需输出，再在冻结帧上选区并原地标注；窗口截图使用 Niri 合成器的窗口独立渲染，而不是桌面截图裁剪。

成功意味着：作者每天愿意用；Niri 用户安装后绑定快捷键即可完成“截到 → 标注 → 复制/保存”；截图结果与冻结帧逐像素一致，窗口截图永远不会混入其他窗口。

## Positioning

一个只围绕 Niri 有文档的 IPC 与合成器 action 构建的截图工具：冻结复用 DMS CLI（可选，缺失时回退 `grim`），窗口截图调用 `niri msg action screenshot-window --id` 得到窗口自身的渲染。它不依赖 Quickshell、不做窗口的桌面裁剪回退，编辑器是独立进程，DMS shell 重启后仍存活。

## Operating Context

- Niri 26.04+ / Wayland；Arch/CachyOS 有本地安装脚本，同时保留通用 CMake 安装；DMS 为可选后端，不是依赖。
- 入口：Niri 键位 spawn、托盘菜单、CLI；编辑器有两种形态——冻结选区上的就地编辑器，以及独立的新建/打开编辑器窗口。
- 产物：复制到 Wayland 剪贴板、保存 PNG、钉图、OCR 文本面板、JSON 配置（`~/.config/niricapture/config.json`，schema v2）。
- CLI 模式：`--capture-region`、`--capture-fullscreen`、`--capture-all`、`--capture-window`、`--list-windows`、`--window-id`、`--editor`、`--file`、`--clipboard`、`--pin`、`--settings`、`--tray`、`--print-config`、`--config`，以及 `--copy`/`--save`。
- 文档：`README.md` 中文默认，`README.en.md` 英文；工程记录在 `docs/`。

## Capabilities and Constraints

**已确认能力**

- 区域 / 当前输出 / 全部输出截图：优先 `dms screenshot output`，缺失时 `grim`；多输出按各自原生像素合成，逻辑空隙保持透明。
- 窗口截图：Niri 原生窗口渲染，支持完全遮挡、非当前工作区、透明窗口与窗口装饰；成功判断要求 PNG 可读且非空。
- 就地编辑器：工具栏直接画在冻结选区上，选区手柄可基于完整冻结帧放大或缩小；跨输出选区按所属输出裁剪。
- 独立编辑器：新建画布、打开图片、画布尺寸/内边距/背景、缩放平移、保存/复制/钉图/OCR。
- 标注：选择移动缩放、画笔、箭头、直线、空心/填充矩形与椭圆、文字（自定义字体）、高亮、马赛克、高斯模糊、序号、取色器；撤销/重做；Shift 等比例、Ctrl 中心缩放、Alt 磁吸。
- 配置 v2：工具/命令顺序与可见性、快捷键、调色板、画布、主题、OCR 语言、截图目录；原子保存、v1 迁移、未知字段保留、更新 schema 只读保护。
- 设置窗口（`--settings`）、托盘（`--tray`，独立 `tray.lock`）、XDG autostart。
- 单实例锁与旧锁恢复；DMS CLI 调用使用参数数组，不经过 shell。

**硬性约束（后续工作必须保持）**

- 只用 Niri 有文档的 JSON IPC 与合成器 action，不猜 API。
- 区域/整屏复用 DMS 冻结（没有 DMS 时回退 grim）；窗口截图绝不从桌面截图裁剪。
- Fail closed：所需冻结帧没齐之前不显示覆盖层。
- 编辑器是独立进程，必须能在 DMS shell 重启后存活。
- 不修改 Niri/DMS 的配置或核心；只能生成片段。
- Niri `screenshot-window --id` 总会覆盖剪贴板且 PNG 异步写入——只能在文档里说明，不得声称能恢复剪贴板或凭退出码判断成功。
- 保留 `LICENSE`、`NOTICE`、`LICENSES/MIT-nirisnap.txt`、`assets/OFL.txt`。
- 能进配置文件的东西都进配置，避免硬编码。
- 原生 Linux 桌面应用（Qt 6 Widgets，Wayland），没有 web 前端；浏览器侧的检查与检测工具不适用于本仓库的 UI。

**明确未定的产品事实**

- 1.0 的受众优先级（作者 vs 新用户）未定。
- 滚动截图、GIF/视频录制、结果缩略图弹窗不在当前范围；1.0 是否仍排除它们未确认。
- 多显示器/混合 DPI 只有合成测试，没有真实双屏端到端记录。

## Brand Commitments

- 名称固定：**NiriCapture**。
- 许可证固定：整体 **AGPL-3.0-only**；派生自 Nirisnap（MIT），声明见 `NOTICE` 与 `LICENSES/MIT-nirisnap.txt`；捆绑的 Neucha 字体为 OFL。
- 不引入 Hyprland 依赖；不依赖 Quickshell 或合成器插件。
- 文档中文默认（`README.md`），英文单列；界面中文默认，`NIRICAPTURE_LANG=zh_CN|en` 可覆盖。
- 对外表述保持事实性：不虚构用户、基准或兼容性声明。

## Evidence on Hand

- `docs/TESTING.md`：实机验证记录，包括 DMS 冻结帧与导出逐像素一致、完全遮挡窗口、非当前工作区窗口、透明窗口、OCR、安装脚本与托盘自启（systemd xdg-autostart generator）。
- `docs/screenshots/`：真实截图与录屏预览（部分仍待替换成当前 UI）。
- `docs/CONFIGURATION.md`：完整配置字段；`docs/EDITOR-ARCHITECTURE.md`：编辑器模块结构。
- Release 资产（v0.6.2 deb）、`packaging/PKGBUILD`、`install-local.sh --check`。
- 实机环境：Arch Linux、Niri 26.04、DMS 2.0.0、Qt 6.12、单输出 1920×1080。
- **不得虚构**：双屏/混合 DPI 实机结果、性能基准、用户评价。

## Product Principles

1. **Niri 原生优先**：只走有文档的 IPC/action；窗口截图宁可失败也不用桌面裁剪凑数。
2. **失败就失败**：冻结帧不完整就不显示覆盖层，不展示可能误导的画面。
3. **日常动作最短**：截图 → 标注 → 复制/保存的路径要短，键盘可用，工具栏不挡内容。
4. **配置驱动**：工具、快捷键、调色板、主题、OCR、路径尽量写入配置文件。
5. **零侵入**：不装插件、不跑守护进程、不改合成器与 DMS 配置，编辑器独立存活。

## Accessibility & Inclusion

- 键盘优先：所有标注工具有快捷键，Esc/Enter 有明确语义；未建立正式的无障碍标准。
- 界面中英双语，状态与错误信息本地化；字号与对比度尚无专项验证。
