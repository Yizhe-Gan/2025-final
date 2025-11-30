# 期末大作业

## 仓库结构

- `cli/`：命令行客户端源代码与 CMake 配置，用于本地交互式操作。
- `src/`：主代码。后端服务实现。
- `test/`：单元/集成测试代码。
- `utils/`：工具脚本与示例数据。
- `web/`：示例前端，用于演示与后端交互的最小网页界面。

## 环境配置（Windows / PowerShell）

前提工具：

- 安装 Visual Studio（含 Desktop development with C++）或至少安装 MSVC 工具链与 Windows SDK。
- 安装 CMake（推荐 3.20+）。
- 安装 Git。
- 推荐使用 `vcpkg` 管理第三方库（jansson、civetweb、sqlite3、pdcurses 等）。

示例 `vcpkg` + CMake 配置与构建步骤（PowerShell）：

```powershell
# 克隆仓库
git clone https://github.com/IcedDog/curriculum.git
cd curriculum

# 获取并 bootstrap vcpkg（若已安装，请跳过）
# git clone https://github.com/microsoft/vcpkg.git C:\vcpkg; C:\vcpkg\bootstrap-vcpkg.bat

# 使用 vcpkg 安装依赖（示例 triplet: x64-windows）
# C:\vcpkg\vcpkg.exe install sqlite3 jansson civetweb pdcurses --triplet x64-windows

# 生成构建系统：指定 vcpkg toolchain
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# 使用 Visual Studio 生成或在命令行构建
cmake --build build --config Debug

# 运行可执行文件（示例路径）
.\build\Debug\curriculum.exe
```

## 其他说明

大量导入数据时，`utils/` 中的 Python 脚本提供了批量插入示例。
