# LabelMinus Qt

LabelMinus Qt 是 LabelMinus 的 C++/Qt 6 移植版本，目标是在 Linux、Windows 与 macOS 上提供可用的 LabelPlus 文本工程编辑体验。

当前版本面向已有的经典 LabelPlus `.txt` 工程文件：程序可以打开文本工程，从工程文件所在目录加载图片，在图像预览区添加、移动和筛选标签，并将修改后的内容保存回 LabelPlus 文本格式。

> 本分支是 Qt 移植分支，旧 WPF 实现已按计划移除。

## 功能概览

- 从图片文件夹新建工程，或打开和保存经典 LabelPlus `.txt` 工程文件。
- 支持从命令行直接打开工程文件。
- 左侧图像预览区支持缩放、翻页、点击添加标签。
- 支持按偏好设置中的修饰键拖动标签坐标，默认使用 `Ctrl`。
- 标签 marker 可按分组配置颜色、大小、字体大小和形状。
- 右侧标签列表支持文本和类别的原地编辑。
- 标签列表支持 `Shift` 连续多选、`Ctrl` 多选、批量删除和批量切换类别。
- 分组筛选同时作用于右侧标签列表和左侧图像预览区。
- 鼠标悬停在图像 marker 上时显示标签文本提示。
- 提供简单的命令式撤销栈，覆盖新增、删除、移动、文本编辑和类别修改等标签编辑操作。
- 支持按间隔自动备份已修改的 LabelPlus 文本工程。
- 偏好设置窗口支持通过系统字体选择器分别调整标签列表和大文本编辑框字体。
- 提供简体中文和英文界面文本，并使用 Qt Linguist 工作流生成翻译资源。
- `preference.json` 支持对界面交互和 marker 样式做运行时调优。

## 当前状态

项目仍处于移植和功能重构阶段，重点是复刻并改进 LabelPlus 文本工程的基础编辑流程。OCR、压缩包读取、平台集成等能力会在后续阶段继续完善。

## 环境要求

- CMake 3.25 或更高版本。
- Ninja，或 Windows 上的 Visual Studio 2022 生成器。
- 支持 C++20 的编译器。
- Qt 6，至少需要 Qt Widgets；开发翻译资源时还需要 Qt Linguist Tools。

## 构建

推荐使用仓库内置的 CMake Presets。Debug 与 Release 均已提供跨平台 preset。

### Linux

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Release 构建：

```bash
cmake --preset linux-release
cmake --build --preset linux-release
```

### Windows

默认 Windows preset 使用 Ninja：

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Release 构建：

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
```

如果不想安装 Ninja，也可以使用 Visual Studio 2022 生成器 preset：

```powershell
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug
ctest --preset windows-vs-debug
```

Visual Studio Release 构建：

```powershell
cmake --preset windows-vs-release
cmake --build --preset windows-vs-release
```

### macOS

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug
ctest --preset macos-debug
```

Release 构建：

```bash
cmake --preset macos-release
cmake --build --preset macos-release
```

如果本机编译器经由 ccache 转发，且 CMake 在检测 `Threads` 时失败，可以临时禁用 ccache 后重试：

```bash
cmake -E env CCACHE_DISABLE=1 cmake --preset linux-debug
cmake -E env CCACHE_DISABLE=1 cmake --build --preset linux-debug
cmake -E env CCACHE_DISABLE=1 ctest --preset linux-debug
```

## 运行

Linux Debug 构建完成后，可直接运行：

```bash
./build/linux/debug/src/labelminus
```

也可以在启动时传入 LabelPlus 文本工程路径：

```bash
./build/linux/debug/src/labelminus /path/to/project.txt
```

Linux Release 构建对应路径为：

```bash
./build/linux/release/src/labelminus
```

Windows 构建会生成 GUI 可执行文件，macOS 构建会生成应用包。

## 偏好设置

运行时界面偏好位于仓库根目录的 `preference.json`，由 `AppPreferences` 统一读取。无效或损坏的配置会回退到默认值，并在状态栏显示常驻警告。

也可以在程序内通过“文件 > 偏好设置”打开偏好设置窗口，使用结构化表单编辑常用选项，或直接调用系统文本编辑器打开 `preference.json`。

当前默认配置示例：

```json
{
  "backupPath": "bak",
  "backupIntervalSeconds": 60,
  "labelMarker": {
    "diameter": 20.0,
    "fontPointSize": 10.0
  },
  "labelTable": {
    "fontFamily": "",
    "fontPointSize": 0.0,
    "maxTextRows": 4
  },
  "labelTextEditor": {
    "fontFamily": "",
    "fontPointSize": 0.0
  },
  "input": {
    "moveLabelModifier": "ctrl"
  },
  "groupStyles": [
    {
      "groupColor": "#ff3835",
      "markerDiameter": 20.0,
      "fontPointSize": 10.0,
      "markerStyle": "circle"
    },
    {
      "groupColor": "#2d59d2",
      "markerDiameter": 20.0,
      "fontPointSize": 10.0,
      "markerStyle": "square"
    },
    {
      "groupColor": "#5f8000",
      "markerDiameter": 20.0,
      "fontPointSize": 10.0,
      "markerStyle": "circle"
    }
  ]
}
```

字段说明：

- `labelMarker.diameter`：默认 marker 直径，单位为屏幕像素，支持浮点数。
- `labelMarker.fontPointSize`：默认 marker 内部序号字号，使用 Qt 字号单位，支持浮点数。
- `labelTable.maxTextRows`：右侧标签列表文本列自动换行后的最大显示行数。
- `labelTable.fontFamily`：右侧标签列表字体。为空时使用系统默认字体。
- `labelTable.fontPointSize`：右侧标签列表字号。为 `0` 时使用系统默认字号。
- `labelTextEditor.fontFamily`：右下角大文本编辑框字体。为空时使用系统默认字体。
- `labelTextEditor.fontPointSize`：右下角大文本编辑框字号。为 `0` 时使用系统默认字号。
- `input.moveLabelModifier`：拖动图像 marker 时需要按住的修饰键，默认 `ctrl`。
- `backupPath`：自动备份目录，默认 `bak`。相对路径会解析到当前工程文件所在目录下，绝对路径会直接使用。
- `backupIntervalSeconds`：自动备份检查间隔，单位为秒，默认 60。
- `groupStyles`：按分组顺序应用的分组样式数组。

当有工程打开且存在未保存修改时，程序会按 `backupIntervalSeconds` 检查是否需要备份。每次备份会在 `backupPath` 中生成一份 LabelPlus 文本工程副本，文件名由当前工程文件名和时间戳组成。

`groupStyles` 中每一项可包含：

- `groupColor`：分组代表色，用于图像 marker、插入分组下拉框、分组筛选菜单和标签列表类别列。
- `markerDiameter`：该分组 marker 的直径。
- `fontPointSize`：该分组 marker 内部序号字号。
- `markerStyle`：marker 形状，当前支持 `circle` 和 `square`。

如果某个分组没有对应的 `groupStyles` 项，图像 marker 会使用黑色圆形和默认大小，文本界面保留默认文字颜色。

## 开发检查

提交前建议运行：

```bash
scripts/check_translations.sh
cmake --build --preset linux-debug --target release_translations
cmake --build --preset linux-debug
ctest --preset linux-debug
```

格式化 C++ 源码：

```bash
find src tests -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 | xargs -0 clang-format -i
```

## 国际化

项目使用 Qt 推荐的 Linguist 工作流：

- 新增用户可见 UI 文本时使用 `tr()`。
- 同步更新 `translations/labelminus_zh_CN.ts` 与 `translations/labelminus_en_US.ts`。
- 使用 `release_translations` 生成 `.qm` 翻译资源。
- 程序启动时会根据系统区域设置自动加载匹配翻译。

## 项目结构

```text
src/core        平台无关的数据模型、LabelPlus 解析与保存、偏好设置、撤销栈
src/ui          Qt Widgets 用户界面
src/services    OCR、压缩包、进程等外部集成预留位置
translations    Qt Linguist 翻译源文件
tests           Qt Test 单元测试
docs            架构与开发文档
scripts         开发辅助脚本
```

更多开发约定请参见：

- `CONTRIBUTING.md`
- `docs/architecture.md`
- `docs/code-walkthrough.md`
- `AGENTS.md`
