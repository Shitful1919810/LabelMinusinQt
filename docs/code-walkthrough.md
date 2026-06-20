# C++ Code Walkthrough

这份文档面向第一次阅读当前 Qt 移植版代码的人。它不替代 `docs/architecture.md`，而是更偏“从哪里开始看、功能之间怎么串起来”。

## 入口与构建目标

程序入口在 `src/main.cpp`：

- 创建 `QApplication`。
- 设置应用名、组织名和版本。
- 调用 `installTranslator()` 从 Qt 资源或可执行文件旁边的 `i18n` 目录加载翻译。
- 用 `QCommandLineParser` 解析可选的工程文件路径参数。
- 创建并显示 `MainWindow`。
- 如果命令行传入了 `.txt` 工程路径，则调用 `MainWindow::openProjectFile()` 自动打开。

可执行目标在 `src/CMakeLists.txt` 中定义。源码被分为三组：

- `LABELMINUS_CORE_SOURCES`：平台无关的数据、解析、偏好和撤销。
- `LABELMINUS_SERVICE_SOURCES`：应用服务层，目前包含工程控制、会话状态、压缩包和 OCR 进程占位实现。
- `LABELMINUS_UI_SOURCES`：Qt Widgets 界面。

构建后会把仓库根目录的 `preference.json` 复制到可执行文件目录，便于运行时读取默认偏好。

## 目录分层

```text
src/core      数据模型、LabelPlus 文本解析保存、偏好设置、撤销栈
src/ui        Qt Widgets 界面、模型/委托、画布、主窗口、偏好窗口
src/services  工程工作流、会话状态、OCR、压缩包、进程等服务层
tests         Qt Test 单元测试
translations  Qt Linguist .ts 翻译源文件
```

核心原则是：`src/core` 不依赖 Qt Widgets，不应该知道按钮、表格、窗口；`src/services` 承接不属于具体控件的应用服务；`src/ui` 可以协调这些对象，但不要把文件格式解析、备份或本机会话存储逻辑散落进 UI。

## 核心数据模型

### `Label`

文件：`src/core/Label.h`、`src/core/Label.cpp`

`Label` 表示单个标签：

- `text`：标签文本。
- `group`：所属类别。
- `position`：归一化坐标，范围约为 `[0, 1]`，相对于图片宽高。
- `deleted`：软删除状态。

坐标使用归一化值是为了让图片缩放、窗口缩放和实际保存格式解耦。

### `Project` 与 `ImageEntry`

文件：`src/core/Project.h`、`src/core/Project.cpp`

`Project` 是当前打开工程的内存表示：

- `images()`：页面列表。
- `groups()`：类别列表。
- `filePath()`：当前 LabelPlus `.txt` 工程路径。
- `sourceName()`：工程来源名称。

`ImageEntry` 表示一页图片：

- `name`：图片文件名。
- `path`：图片绝对路径。
- `labels`：该页标签。

当前打开工程由 `ProjectController` 持有。UI 侧通常通过 `MainWindow::project()` 访问它，再修改
`project().images()[page].labels` 或 `project().groups()`。

### `LabelPlusDocument`

文件：`src/core/LabelPlusDocument.h`、`src/core/LabelPlusDocument.cpp`

负责经典 LabelPlus `.txt` 工程格式：

- `loadFromFile()`：读取并解析文本工程，得到 `Project`。
- `saveToFile()`：把 `Project` 序列化回 LabelPlus 文本格式。

如果以后要兼容更多 LabelPlus 格式细节，优先从这里改，而不是在 `MainWindow` 里做字符串拼接。

### `AppPreferences`

文件：`src/core/AppPreferences.h`、`src/core/AppPreferences.cpp`

负责读取 `preference.json` 并提供类型化访问：

- marker 默认大小、字体大小。
- 分组样式 `groupStyles`。
- 标签列表字体和最大行数。
- 右下角文本编辑框字体。
- marker 拖动修饰键。
- 自动备份路径和间隔。

解析错误不会直接弹窗，而是返回 `AppPreferencesLoadResult`，其中包含 `warnings`。主窗口把这些警告显示在状态栏常驻 warning label 中。

新增偏好项时通常需要同步修改：

- `AppPreferences.h/.cpp`
- `preference.json`
- `PreferenceDialog`
- `README.md`
- `docs/architecture.md`
- `tests/LabelTests.cpp`

如果新增了 UI 文本，还要同步 `translations/labelminus_zh_CN.ts` 和 `translations/labelminus_en_US.ts`。

### `UndoStack`

文件：`src/core/UndoStack.h`、`src/core/UndoStack.cpp`

这是一个轻量命令式撤销栈。每个命令包含：

- `text`：命令名称。
- `undo`：撤销函数。

当前只实现撤销，没有 redo。标签编辑由 `LabelEditController` 负责把对应命令压栈。

## 服务层

### `ProjectController`

文件：`src/services/ProjectController.h`、`src/services/ProjectController.cpp`

`ProjectController` 是当前工程工作流的入口，负责：

- 持有打开中的 `Project`。
- 通过 `LabelPlusDocument` 打开、保存、另存工程。
- 从图片目录创建新的 LabelPlus 文本工程。
- 维护 dirty 状态。
- 在存在未保存修改时执行自动备份。

这样 `MainWindow` 不需要直接管理“工程是否已修改”“备份文件写到哪里”“保存失败时如何报告错误”等非控件细节。主窗口仍负责弹窗、状态栏提示和菜单动作，因为这些属于 UI 反馈。

### `LabelEditController`

文件：`src/services/LabelEditController.h`、`src/services/LabelEditController.cpp`

`LabelEditController` 负责标签数据编辑和 undo 注册：

- 新增标签。
- 删除标签。
- 修改文本、类别和坐标。
- 批量切换类别。
- 拖拽重排标签顺序。
- undo 回放时恢复旧文本、旧类别、旧坐标、旧顺序或旧删除状态。

它不依赖 Qt Widgets，也不直接操作表格、画布或文本框。需要更新 UI 时，它通过 `MainWindow` 注册的回调通知“某页某个标签应被选中”或“某页选区应被清空”。这样标签编辑规则集中在服务层，窗口层只保留刷新和交互反馈。

### `SessionStateStore`

文件：`src/services/SessionStateStore.h`、`src/services/SessionStateStore.cpp`

`SessionStateStore` 负责使用 `QSettings` 保存本机状态：

- 主窗口几何信息。
- 左右 splitter 状态。
- 每个工程上次停留的图片页。
- 缩放比例、视图中心和选中标签。

这些状态是“本机使用习惯”，不写入 `preference.json`，也不写入 LabelPlus 工程文本。工程下次打开时，`MainWindow` 会读取这些状态并做边界检查，例如图片页被外部删除时会退回到仍然存在的页。

## 主窗口：`MainWindow`

文件：`src/ui/MainWindow.h`、`src/ui/MainWindow.cpp`

`MainWindow` 是当前 UI 编排中心，也是最值得先读的文件。它负责：

- 创建菜单、工具栏区域和左右布局。
- 响应打开、保存、新建工程等菜单动作，并把工程读写交给 `ProjectController`。
- 连接 `ImageCanvas`、`LabelTableModel`、右下角文本编辑框和类别控件。
- 管理当前页 `m_currentImageIndex` 和当前标签 `m_currentLabelIndex`。
- 调用 `ProjectController` 做工程读写、dirty 状态和自动备份。
- 调用 `SessionStateStore` 恢复和保存窗口布局、每个工程的查看位置。
- 把 `AppPreferences` 应用到画布、表格、文本框和自动备份计时器。
- 把标签编辑请求交给 `LabelEditController`，再根据结果刷新表格、画布和选区。

### 常见入口函数

- `createActions()`：创建菜单 action。
- `createMenus()`：组装菜单栏。
- `createCentralWidget()`：创建主界面左右区域。
- `newProject()`：从图片文件夹创建新工程。
- `openProjectFile()`：打开 LabelPlus 文本工程。
- `saveProject()` / `saveProjectAs()`：保存工程。
- `refreshProjectUi()`：工程级 UI 刷新。
- `refreshImageUi()`：当前图片、标签表格、画布刷新。
- `refreshGroupUi()`：类别相关 UI 刷新。
- `applyPreferences()`：应用偏好设置窗口或启动读取到的配置。
- `restoreProjectSessionState()` / `saveProjectSessionState()`：恢复和保存每个工程的查看位置。

### 编辑数据的基本路径

以“在图片上点击新增标签”为例：

1. `ImageCanvas` 判断点击位置并发出 `labelCreateRequested(QPointF)`。
2. `MainWindow::addLabel()` 收到信号。
3. `MainWindow::addLabel()` 调用 `LabelEditController::addLabel()`。
4. `LabelEditController` 修改当前 `ImageEntry::labels`，注册撤销命令，并标记工程已修改。
5. `MainWindow` 根据返回结果刷新表格、画布和当前选中标签。

以“右侧表格原地修改文本/类别”为例：

1. `LabelTableModel::setData()` 发现文本或类别变化。
2. 发出 `labelEdited(sourceIndex, column, oldValue, newValue)`。
3. `MainWindow::updateLabelFromTable()` 注册对应 undo，并刷新画布与选区。
4. 实际数据修改发生在 `LabelTableModel::setData()`；后续如果表格编辑路径继续重构，应考虑让它也统一走 `LabelEditController`。

如果你要新增会修改工程内容的功能，优先确认它是否走了 `markDirty()` 和 `UndoStack`。

## 图像预览：`ImageCanvas`

文件：`src/ui/ImageCanvas.h`、`src/ui/ImageCanvas.cpp`

`ImageCanvas` 继承 `QGraphicsView`，用于左侧图片预览和 marker 交互。

它负责：

- 加载并显示当前图片。
- 按缩放比例显示图片。
- 绘制标签 marker。
- 根据分组过滤隐藏 marker。
- 鼠标点击请求新增标签。
- 按偏好中的修饰键拖动 marker 坐标。
- 鼠标悬停 marker 时显示标签文本提示。
- 在焦点位于画布时触发 undo 请求。

注意：`ImageCanvas` 不直接修改 `Project`。它持有当前页标签的绘制快照，通过信号告诉 `MainWindow` 用户想做什么：

- `labelCreateRequested`
- `labelMoveRequested`
- `labelSelected`
- `undoRequested`
- `zoomPercentChanged`

这种设计让画布只处理交互和绘制，真实工程数据仍由 `ProjectController` 持有，并由主窗口协调更新。当前页标签变化时，主窗口通过 `refreshCanvasLabels()` 更新画布快照；整页切换时才重新载入图片。

## 右侧标签列表

### `LabelTableModel`

文件：`src/ui/LabelTableModel.h`、`src/ui/LabelTableModel.cpp`

这是右侧 label 列表的 `QAbstractTableModel`。它不是拷贝一份标签，而是持有当前图片 `QVector<Label>*` 指针。

主要职责：

- 把当前页标签映射成表格行。
- 维护 `m_visibleRows`，实现分组过滤。
- 提供三列数据：序号、文本、类别。
- 给类别列返回分组颜色。
- 支持拖拽排序，并通过 `labelsReorderRequested` 通知主窗口。

`sourceIndexForRow()` 和 `rowForSourceIndex()` 很重要：因为表格可能被分组过滤，显示行号和真实标签下标并不总是相同。

### `LabelEditDelegates`

文件：`src/ui/LabelEditDelegates.h`、`src/ui/LabelEditDelegates.cpp`

提供表格原地编辑控件：

- `LabelTextDelegate`：文本列使用 `QPlainTextEdit`。
- `LabelGroupDelegate`：类别列使用 `QComboBox`。

如果以后要改善表格编辑体验，多半从这里和 `LabelTableModel::setData()` 一起看。

## 分组筛选：`GroupFilterComboBox`

文件：`src/ui/GroupFilterComboBox.h`、`src/ui/GroupFilterComboBox.cpp`

这是右上角的多选类别筛选框。它看起来像下拉框，但内部用菜单和 checkbox 实现：

- 支持全选、清空。
- 每个 group 前有复选框。
- 单选时显示 group 名称，多选时显示数量。
- 根据 `groupStyles` 给条目着色。

当筛选变化时，它发出 `selectedGroupsChanged()`。`MainWindow` 随后同时更新：

- `LabelTableModel` 的过滤。
- `ImageCanvas` 的可见 marker 分组。

## 偏好设置窗口：`PreferenceDialog`

文件：`src/ui/PreferenceDialog.h`、`src/ui/PreferenceDialog.cpp`

偏好窗口直接编辑 `preference.json` 对应的结构化内容，并提供 JSON 预览。

当前包含：

- marker 默认大小。
- 标签列表最大行数。
- 标签列表字体。
- 大文本编辑框字体。
- marker 拖动修饰键。
- 自动备份路径和间隔。
- 分组样式表，包括颜色、marker 大小、字体大小和形状。

字体选择使用 `QFontDialog`，但最终仍写回：

- `labelTable.fontFamily`
- `labelTable.fontPointSize`
- `labelTextEditor.fontFamily`
- `labelTextEditor.fontPointSize`

窗口点击“应用”时不一定保存文件，但会把当前 UI 生成的 JSON 交给 `AppPreferences::loadFromJson()`，再通过 `preferencesApplied()` 发给主窗口。

## 自动备份

自动备份由 `MainWindow` 和 `ProjectController` 分工完成：

- `configureBackupTimer()` 根据 `backupIntervalSeconds` 配置定时器。
- `MainWindow::markDirty()` 将修改状态交给 `ProjectController`。
- `ProjectController::markDirty()` 会记录工程已修改且存在待备份内容。
- `MainWindow::performAutoBackup()` 负责定时触发和展示结果。
- `ProjectController::performAutoBackup()` 在工程已打开、存在未保存修改且有待备份标记时保存一份副本。

备份路径由 `backupPath` 决定：

- 相对路径：相对于当前工程 `.txt` 所在目录。
- 绝对路径：直接使用。

备份文件名包含原工程文件名和时间戳。

## 国际化

翻译资源在 `translations/` 下：

- `labelminus_zh_CN.ts`
- `labelminus_en_US.ts`

规则：

- 新增用户可见 UI 文本时使用 `tr()`。
- 同步更新两个 `.ts` 文件。
- 运行 `scripts/check_translations.sh` 检查遗漏。
- 构建时 CMake 会生成 `.qm` 并打进资源路径 `/i18n`。

`main.cpp` 会在创建 `MainWindow` 前安装翻译器，所以 UI 构造期间的 `tr()` 能拿到当前语言。

## 测试

当前测试入口在 `tests/LabelTests.cpp`，覆盖：

- `Label` 坐标裁剪。
- LabelPlus 文本工程读写 round-trip。
- `AppPreferences` 对浮点 marker、输入修饰键、备份、字体、分组样式的解析。
- 损坏偏好文件的默认回退。

新增 core 行为时优先补这里。UI 行为目前主要靠手动验证，后续如果引入更复杂的交互，可以考虑加 Qt GUI 测试。

## 推荐阅读顺序

如果你想快速熟悉项目，可以按这个顺序读：

1. `src/main.cpp`
2. `src/core/Project.h`、`src/core/Label.h`
3. `src/core/LabelPlusDocument.cpp`
4. `src/ui/MainWindow.h`
5. `src/ui/MainWindow.cpp` 的构造、`createCentralWidget()`、`openProjectFile()`、`refreshImageUi()`
6. `src/ui/ImageCanvas.cpp`
7. `src/ui/LabelTableModel.cpp`
8. `src/core/AppPreferences.cpp`
9. `src/ui/PreferenceDialog.cpp`
10. `tests/LabelTests.cpp`

读 `MainWindow.cpp` 时不用从头到尾硬啃。更有效的方法是围绕一个功能追信号和槽，例如“新增标签”“修改类别”“拖动 marker”“保存工程”。

## 修改功能时的落点

- 新增文件格式能力：优先看 `LabelPlusDocument`。
- 新增项目数据字段：先改 `Label` / `Project`，再改读写和 UI 展示。
- 新增可配置行为：改 `AppPreferences`、`preference.json`、`PreferenceDialog`、README、测试。
- 新增画布交互：改 `ImageCanvas` 发信号，再由 `MainWindow` 改数据。
- 新增标签表格行为：改 `LabelTableModel`、必要时改 `LabelEditDelegates`。
- 新增标签编辑操作：优先走 `LabelEditController`，并确保注册 `UndoStack` 命令。
- 新增其他会修改工程的操作：必须 `markDirty()`，并注册 `UndoStack` 命令。
- 新增 UI 文本：必须 `tr()`，并同步中英文翻译。
