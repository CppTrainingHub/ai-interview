#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
C++ Interview System - Cross-platform Build Script
支持 Windows、Linux 和 macOS 的统一构建脚本
"""

import os
import sys
import subprocess
import argparse
import platform
import shutil
from pathlib import Path

# Windows 控制台编码设置
if sys.platform == 'win32':
    # 设置控制台为 UTF-8 模式
    try:
        # Python 3.7+ 支持
        if hasattr(sys.stdout, 'reconfigure'):
            sys.stdout.reconfigure(encoding='utf-8', errors='replace')
            sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

    # 设置环境变量强制 UTF-8
    os.environ['PYTHONIOENCODING'] = 'utf-8'


class Colors:
    """终端颜色常量"""
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

    @staticmethod
    def is_supported():
        """检查终端是否支持颜色"""
        return sys.platform != 'win32' or 'ANSICON' in os.environ


def print_colored(message, color=Colors.OKBLUE):
    """打印带颜色的消息"""
    if Colors.is_supported():
        print(f"{color}{message}{Colors.ENDC}")
    else:
        print(message)


def print_header(message):
    """打印标题"""
    print()
    print_colored("=" * 60, Colors.HEADER)
    print_colored(message, Colors.HEADER)
    print_colored("=" * 60, Colors.HEADER)


def print_success(message):
    """打印成功消息"""
    print_colored(f"✓ {message}", Colors.OKGREEN)


def print_error(message):
    """打印错误消息"""
    print_colored(f"✗ {message}", Colors.FAIL)


def print_warning(message):
    """打印警告消息"""
    print_colored(f"⚠ {message}", Colors.WARNING)


def print_info(message):
    """打印信息"""
    print_colored(f"→ {message}", Colors.OKCYAN)


def run_command(cmd, cwd=None, shell=False, check=True):
    """执行命令并实时输出"""
    print_info(f"执行命令: {' '.join(cmd) if isinstance(cmd, list) else cmd}")

    try:
        if shell and isinstance(cmd, list):
            cmd = ' '.join(cmd)

        # 在 Windows 上使用 UTF-8 编码，避免 GBK 解码错误
        encoding = 'utf-8' if sys.platform == 'win32' else None

        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=cwd,
            shell=shell,
            encoding=encoding,
            errors='replace',  # 替换无法解码的字符
            bufsize=1,
            universal_newlines=True
        )

        # 实时输出
        for line in process.stdout:
            print(line, end='')

        process.wait()

        if check and process.returncode != 0:
            raise subprocess.CalledProcessError(process.returncode, cmd)

        return process.returncode

    except subprocess.CalledProcessError as e:
        print_error(f"命令执行失败 (返回码: {e.returncode})")
        if check:
            sys.exit(1)
        return e.returncode
    except UnicodeDecodeError as e:
        print_error(f"编码错误: {e}")
        print_warning("尝试使用 errors='ignore' 模式继续...")
        if check:
            sys.exit(1)
        return -1
    except Exception as e:
        print_error(f"执行命令时发生错误: {e}")
        if check:
            sys.exit(1)
        return -1


class BuildSystem:
    """构建系统类"""

    def __init__(self, args):
        self.args = args
        self.root_dir = Path(__file__).parent.absolute()
        self.build_dir = self.root_dir / "build"
        self.platform = platform.system()
        self.vcpkg_root = None

    def check_environment(self):
        """检查构建环境"""
        print_header("检查构建环境")

        # 检查 Python 版本
        python_version = sys.version_info
        print_info(f"Python 版本: {python_version.major}.{python_version.minor}.{python_version.micro}")
        if python_version < (3, 6):
            print_error("需要 Python 3.6 或更高版本")
            return False
        print_success("Python 版本检查通过")

        # 检查平台
        print_info(f"操作系统: {self.platform}")
        if self.platform not in ['Windows', 'Linux', 'Darwin']:
            print_warning(f"未测试的平台: {self.platform}")

        # 检查 CMake
        try:
            result = subprocess.run(['cmake', '--version'],
                                  capture_output=True, text=True, check=True)
            version = result.stdout.split('\n')[0]
            print_info(f"CMake: {version}")
            print_success("CMake 已安装")
        except FileNotFoundError:
            print_error("未找到 CMake，请先安装 CMake 3.20+")
            print_info("下载地址: https://cmake.org/download/")
            return False
        except subprocess.CalledProcessError:
            print_error("CMake 检查失败")
            return False

        # 检查编译器
        if not self.check_compiler():
            return False

        # 检查 vcpkg
        if not self.check_vcpkg():
            return False

        print_success("环境检查完成！")
        return True

    def check_compiler(self):
        """检查编译器"""
        if self.platform == 'Windows':
            # 检查 MSVC
            try:
                # 尝试找到 Visual Studio
                result = subprocess.run(
                    ['where', 'cl'],
                    capture_output=True,
                    text=True,
                    shell=True
                )
                if result.returncode == 0:
                    print_success("找到 MSVC 编译器")
                    return True
                else:
                    print_warning("未找到 MSVC，将使用 CMake 默认编译器")
                    return True
            except Exception:
                print_warning("无法检测编译器，继续构建...")
                return True

        elif self.platform in ['Linux', 'Darwin']:
            # 检查 g++ 或 clang++
            for compiler in ['g++', 'clang++']:
                try:
                    result = subprocess.run(
                        [compiler, '--version'],
                        capture_output=True,
                        text=True,
                        check=True
                    )
                    version = result.stdout.split('\n')[0]
                    print_info(f"编译器: {version}")
                    print_success(f"找到 {compiler} 编译器")
                    return True
                except FileNotFoundError:
                    continue

            print_error("未找到 g++ 或 clang++ 编译器")
            print_info("Ubuntu/Debian: sudo apt install g++")
            print_info("macOS: xcode-select --install")
            return False

        return True

    def check_vcpkg(self):
        """检查 vcpkg"""
        # 检查环境变量
        vcpkg_root = os.environ.get('VCPKG_ROOT')

        if vcpkg_root:
            vcpkg_path = Path(vcpkg_root)
            if vcpkg_path.exists():
                self.vcpkg_root = vcpkg_path
                print_info(f"VCPKG_ROOT: {vcpkg_root}")
                print_success("找到 vcpkg")
                return True

        # 尝试常见位置
        common_paths = [
            Path.home() / 'vcpkg',
            Path('/opt/vcpkg'),
            Path('C:/vcpkg'),
        ]

        for path in common_paths:
            if path.exists():
                self.vcpkg_root = path
                print_info(f"在 {path} 找到 vcpkg")
                print_success("找到 vcpkg")
                return True

        print_error("未找到 vcpkg")
        print_info("请设置 VCPKG_ROOT 环境变量或安装 vcpkg:")
        print_info("  git clone https://github.com/Microsoft/vcpkg.git")
        print_info("  cd vcpkg && ./bootstrap-vcpkg.sh  # Linux/macOS")
        print_info("  cd vcpkg && .\\bootstrap-vcpkg.bat  # Windows")
        return False

    def clean_build(self):
        """清理构建目录"""
        print_header("清理构建目录")

        if self.build_dir.exists():
            print_info(f"删除: {self.build_dir}")
            try:
                shutil.rmtree(self.build_dir)
                print_success("构建目录已清理")
            except Exception as e:
                print_error(f"清理失败: {e}")
                return False
        else:
            print_info("构建目录不存在，跳过清理")

        return True

    def configure(self):
        """配置 CMake"""
        print_header("配置 CMake")

        # 创建构建目录
        self.build_dir.mkdir(exist_ok=True)

        # 构建 CMake 命令
        cmake_cmd = ['cmake', '..']

        # 添加 vcpkg toolchain
        if self.vcpkg_root:
            toolchain_file = self.vcpkg_root / 'scripts' / 'buildsystems' / 'vcpkg.cmake'
            cmake_cmd.append(f'-DCMAKE_TOOLCHAIN_FILE={toolchain_file}')

        # 添加构建类型
        build_type = self.args.config
        cmake_cmd.append(f'-DCMAKE_BUILD_TYPE={build_type}')

        # Windows 特定配置
        if self.platform == 'Windows':
            cmake_cmd.extend(['-A', 'x64'])

        # 执行配置
        return run_command(cmake_cmd, cwd=self.build_dir, check=True) == 0

    def build(self):
        """编译项目"""
        print_header("编译项目")

        # 构建命令
        cmake_cmd = ['cmake', '--build', '.']

        # 添加配置
        cmake_cmd.extend(['--config', self.args.config])

        # 添加并行编译
        if self.args.jobs > 0:
            if self.platform == 'Windows':
                cmake_cmd.extend(['--', '/m'])
            else:
                cmake_cmd.extend(['--', f'-j{self.args.jobs}'])

        # 执行编译
        return run_command(cmake_cmd, cwd=self.build_dir, check=True) == 0

    def install(self):
        """安装项目"""
        print_header("安装项目")

        cmake_cmd = ['cmake', '--install', '.', '--config', self.args.config]

        if self.args.prefix:
            cmake_cmd.extend(['--prefix', self.args.prefix])

        return run_command(cmake_cmd, cwd=self.build_dir, check=True) == 0

    def get_executable_path(self):
        """获取可执行文件路径"""
        if self.platform == 'Windows':
            return self.build_dir / self.args.config / 'CppInterviewSystem.exe'
        else:
            return self.build_dir / 'CppInterviewSystem'

    def run_executable(self):
        """运行可执行文件"""
        print_header("运行程序")

        exe_path = self.get_executable_path()

        if not exe_path.exists():
            print_error(f"未找到可执行文件: {exe_path}")
            return False

        print_info(f"运行: {exe_path}")

        # 构建运行命令
        run_cmd = [str(exe_path)]
        if self.args.run_args:
            run_cmd.extend(self.args.run_args)

        return run_command(run_cmd, check=False) == 0

    def print_summary(self):
        """打印构建总结"""
        print_header("构建完成")

        exe_path = self.get_executable_path()

        print_success(f"可执行文件: {exe_path}")


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='C++ Interview System - 跨平台构建脚本',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python build.py                    # 默认构建
  python build.py --clean            # 清理后构建
  python build.py --config Debug     # 调试版本
  python build.py --run              # 构建并运行
  python build.py --run -- --help    # 运行并传递参数
        """
    )

    parser.add_argument(
        '--clean',
        action='store_true',
        help='构建前清理构建目录'
    )

    parser.add_argument(
        '--config',
        choices=['Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel'],
        default='Release',
        help='构建配置 (默认: Release)'
    )

    parser.add_argument(
        '--jobs', '-j',
        type=int,
        default=0,
        help='并行编译任务数 (0=自动)'
    )

    parser.add_argument(
        '--run',
        action='store_true',
        help='构建后运行程序'
    )

    parser.add_argument(
        '--install',
        action='store_true',
        help='安装程序'
    )

    parser.add_argument(
        '--prefix',
        type=str,
        help='安装路径前缀'
    )

    parser.add_argument(
        'run_args',
        nargs='*',
        help='传递给程序的参数 (需要 --run)'
    )

    args = parser.parse_args()

    # 如果没有 --run 但有 run_args，提示错误
    if args.run_args and not args.run:
        print_error("程序参数需要配合 --run 使用")
        sys.exit(1)

    # 自动检测 CPU 核心数
    if args.jobs == 0:
        import multiprocessing
        args.jobs = multiprocessing.cpu_count()

    # 创建构建系统实例
    build_system = BuildSystem(args)

    # 显示欢迎信息
    print_header("C++ Interview System - Build Script")
    print_info(f"Python: {sys.version.split()[0]}")
    print_info(f"平台: {platform.system()} {platform.release()}")
    print_info(f"配置: {args.config}")
    print_info(f"并行任务: {args.jobs}")

    # 检查环境
    if not build_system.check_environment():
        print_error("环境检查失败，请修复后重试")
        sys.exit(1)

    # 清理（如果需要）
    if args.clean:
        if not build_system.clean_build():
            sys.exit(1)

    # 配置
    if not build_system.configure():
        print_error("CMake 配置失败")
        sys.exit(1)

    # 编译
    if not build_system.build():
        print_error("编译失败")
        sys.exit(1)

    # 安装（如果需要）
    if args.install:
        if not build_system.install():
            print_error("安装失败")
            sys.exit(1)

    # 打印总结
    build_system.print_summary()

    # 运行（如果需要）
    if args.run:
        build_system.run_executable()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print()
        print_warning("用户中断")
        sys.exit(130)
    except Exception as e:
        print_error(f"发生未预期的错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
