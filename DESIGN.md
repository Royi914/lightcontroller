# Light Controller — 设计文档

## 项目概述

基于 Qt6 C++ 的灯光控制软件，支持 Art-Net（网络）和 DMX512（USB）协议。提供灯具库管理、2D/3D 舞台布局、多宇宙地址管理、实时参数控制、自定义通道值域、色轮调色等功能。

---

## 一、架构总览

```
main.cpp
  └─ MainWindow
       ├─ 菜单栏：文件 / 灯库 / 地址码 / 程序 / 窗口 / 时间线
       ├─ 左侧面板（QStackedWidget）
       │   ├─ 灯库模式：灯库列表 + 已添加灯具 + 输出设置
       │   └─ 控制模式：动态通道控件（滑动条 / 下拉框）+ 光束朝向 + 调色盘
       ├─ 右侧主区域（QStackedWidget）
       │   ├─ 2D/3D 视图 + 时间线
       │   ├─ 地址码页面（512通道表格 + 多域管理）
       │   └─ 被顶层全屏灯库页覆盖
       └─ 全屏灯库页（m_masterStack）
            ├─ 左侧工具栏：新建/打开/保存/另存为/返回
            └─ 右侧QStackedWidget：
                ├─ 列表页：灯具型号列表（双击编辑）
                └─ 详情页：左参数预览 + 右通道表编辑
```

---

## 二、数据流

```
用户操作（滑块/下拉框/拖拽）
  │
  ▼
Fixture::setChannel(ch, value)
  │
  ▼
Universe::render()          ← HTP 合并所有灯具到 512 字节
  │
  ├──→ ArtNetSender::sendDmx()       ← UDP 广播 ArtDmx 数据包
  └──→ DMXUSBWidget::writeUniverse() ← USB 串口输出 DMX512 波形
```

---

## 三、核心类职责

| 类 | 文件 | 职责 |
|---|---|---|
| **ChannelRange** | protocol/Fixture.h | 通道自定义值域（功能名 + 值范围） |
| **FixtureDef** | protocol/Fixture.h | 灯具型号模板（名称、通道数、每通道名、自定义值域） |
| **Fixture** | protocol/Fixture.h | 灯具实例（绑定 DMX 地址 + 宇宙，存储通道值） |
| **Universe** | protocol/Universe.h | 一个 DMX 宇宙（512 字节、灯具列表、HTP 合并） |
| **ArtNetSender** | protocol/ArtNetSender.h | Art-Net 协议发送（ArtDmx 封包，UDP） |
| **ArtNetReceiver** | protocol/ArtNetReceiver.h | Art-Net 协议接收（解析 OpCode，提取 DMX） |
| **FixtureItem** | fixtureitem.h | 2D 视图白色圆形，可拖拽，关联 Fixture |
| **Globe3D** | globe3d.h | 3D 视图（OpenGL 球体 + 轨道相机 + 坐标轴） |
| **DropView** | dropview.h | 自定义 QGraphicsView，接收灯库拖放 |
| **DragLibraryList** | draglibrarylist.h | 灯库列表，发起拖拽（MIME 数据） |
| **AddressPage** | addresspage.h | 地址码页面（512 通道网格 + 多域） |
| **LibraryPage** | librarypage.h | 全屏灯库页（列表 + 编辑 + 导入导出） |
| **FixtureDialog** | fixturedialog.h | 添加灯具弹窗（名称 + 通道参数 + 自定义值域） |
| **RangeDialog** | fixturedialog.h | 通道值域编辑子窗口 |
| **ColorWheel** | colorwheel.h | HSV 色轮 + 十六进制输入 |
| **BeamWidget** | beamwidget.h | 光束朝向 3D 立方体（等轴透视 + 虚线隐藏边） |
| **MainWindow** | mainwindow.h | 主窗口：菜单、布局、事件协调 |

---

## 四、文件清单

```
D:\light controller app\
├── CMakeLists.txt                # CMake 构建配置
├── DESIGN.md                     # 本文档
├── main.cpp                      # 入口
├── mainwindow.ui                 # 主窗口 UI（菜单、布局、控件）
├── mainwindow.h/.cpp             # 主窗口逻辑
├── dropview.h                    # 自定义 GraphicsView（接收拖放）
├── draglibrarylist.h             # 自定义 ListWidget（发起拖拽）
├── fixtureitem.h/.cpp            # 2D 视图灯具圆形
├── fixturedialog.h               # 添加灯具弹窗 + 值域编辑
├── globe3d.h/.cpp                # 3D OpenGL 视图
├── addresspage.h                 # 地址码页面（512格 + 域管理）
├── librarypage.h                 # 全屏灯库页（列表 + 编辑 + 导入导出）
├── colorwheel.h                  # HSV 色轮弹窗
├── beamwidget.h                  # 光束朝向 3D 立方体
└── protocol/
    ├── ArtNetCommon.h            # Art-Net 协议常量
    ├── ArtNetSender.h/.cpp       # Art-Net 发送
    ├── ArtNetReceiver.h/.cpp     # Art-Net 接收
    ├── Fixture.h/.cpp            # 灯具模型（ChannelRange + FixtureDef + Fixture）
    ├── Universe.h/.cpp           # 宇宙管理器
    ├── dmxinterface.h/.cpp       # USB DMX 抽象层
    ├── enttecdmxusbopen.h/.cpp   # DMX512 波形生成
    ├── libftdi-interface.h/.cpp  # FTDI USB 后端
    ├── dmxusbwidget.h/.cpp       # USB 设备枚举
    └── qlcmacros.h               # 宏定义
```

---

## 五、功能总览

| 功能 | 状态 | 说明 |
|------|------|------|
| Art-Net 发送/接收 | ✅ | UDP 6454，广播/单播 |
| DMX512 USB | ✅ | Enttec Open DMX + libftdi |
| 灯库管理 | ✅ | 新建/打开/保存/另存为 JSON |
| 灯具拖拽 | ✅ | 灯库→2D视图，松手位置放置 |
| 2D 视图 | ✅ | 白色圆形，可拖拽、选中、Delete 删除 |
| 3D 视图 | ✅ | OpenGL 球体，右键旋转/中键平移/滚轮缩放 |
| 2D/3D 切换 | ✅ | 视图左上角按钮 |
| 自定义通道值域 | ✅ | 图案盘等多状态通道，显示为下拉框 |
| 控制面板 | ✅ | 动态生成：普通通道=滑块+文本框，自定义=下拉框 |
| 色轮调色 | ✅ | HSV 色轮 + HEX 输入，自动填 R/G/B 通道 |
| 光束朝向 | ✅ | 等轴立方体，虚线隐藏边 |
| 多宇宙(域) | ✅ | 独立 512 通道 + 灯具列表，新建域/切换域 |
| 地址码页面 | ✅ | 512 通道网格 32列×16行，实时刷新 |
| 灯具列表（左侧） | ✅ | 单击高亮、双击控制面板、Delete 删除 |
| 性能优化 | ✅ | 地址码页面关闭时不刷新 UI，Art-Net diff 发送 |

---

## 六、交互设计

### 灯库
- **拖拽**：灯库拖到 2D 视图任意位置松手即放置
- **双击**或**+添加**：自动排列到视图
- **新建**：弹窗输入名称 → 详情页编辑通道
- **详情页**：左侧参数预览(实时同步)，右侧通道表(参数名+完成+自定义+删除)
- **双击列表行**：进入编辑
- **保存**：提交到灯库并同步主页面
- **另存为/打开**：JSON 文件导出/导入

### 灯具选择与控制
- **列表单击**：高亮圆圈(不跳面板)
- **列表双击**：进入控制面板
- **视图单击圆圈**：进入控制面板
- **视图拖拽圆圈**：自由移动
- **Delete 键**：删除灯具，自动退出控制面板

### 输出方式
- Art-Net（无线）：默认广播 192.168.1.255，可指定 IP
- DMX USB（有线）：自动检测 USB 设备

---

## 七、构建与打包

### 环境
- Qt 6.11.1 + MinGW 13.1.0
- CMake + Ninja
- MSYS2: libusb, libftdi1

### 构建
```bash
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### 打包
```bash
windeployqt LightController.exe --no-translations --no-svg
cp /c/msys64/mingw64/bin/libusb-1.0.dll .
cp /c/msys64/mingw64/bin/libftdi1.dll .
```
