import os
import subprocess


def run_make_clean(directory: str):
    """在指定目录执行make clean命令"""
    try:
        # 检查目录中是否存在Makefile
        makefile_path = os.path.join(directory, "Makefile")
        if not os.path.exists(makefile_path):
            return False, f"No Makefile found in {directory}"

        # 执行make clean命令
        # 保存当前工作目录
        original_dir = os.getcwd()
        try:

            # 切换到目标目录
            os.chdir(directory)
            print(f"切换到目录: {directory}")

            # 执行make clean命令
            result = subprocess.run(
                ["make", "clean"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=30,  # 保持超时设置
            )

            # 切换回原工作目录
            os.chdir(original_dir)

        except subprocess.TimeoutExpired:
            # 确保即使超时也能切换回原目录
            os.chdir(original_dir)
            return False, f"make clean timed out in {directory} (exceeded 30 seconds)"
        except Exception as e:
            # 发生其他异常时也切换回原目录
            os.chdir(original_dir)
            return False, f"Error executing make clean in {directory}: {str(e)}"

        # 捕获并记录标准输出，便于调试
        output = f"Output: {result.stdout}\n" if result.stdout else ""

        if result.returncode == 0:
            return True, f"Successfully ran 'make clean' in {directory}. {output}"
        else:
            return (
                False,
                f"Failed to run 'make clean' in {directory}. {output}Error: {result.stderr}",
            )

    except Exception as e:
        return False, f"Error processing {directory}: {str(e)}"


def recursive_make_clean(start_dir: str):
    """递归遍历目录并执行make clean"""
    # 确保起始目录存在
    if not os.path.isdir(start_dir):
        print(f"Error: {start_dir} is not a valid directory")
        return

    # 遍历目录树
    for root, _, _ in os.walk(start_dir):
        # 在当前目录执行make clean
        success, message = run_make_clean(root)
        if success:
            print(f"✅ {message}")
        else:
            # 只打印有错误的消息，忽略没有Makefile的情况
            if "No Makefile found" not in message:
                print(f"❌ {message}")


if __name__ == "__main__":
    # 处理bouffalo_sdk_vmg0/examples目录
    bouffalo_dir = os.path.join(os.getcwd(), "bouffalo_sdk_vmg0", "examples")
    print(f"Starting recursive 'make clean' in: {bouffalo_dir}\n")
    if os.path.isdir(bouffalo_dir):
        recursive_make_clean(bouffalo_dir)
    else:
        print(f"❌ Directory not found: {bouffalo_dir}\n")

    # 处理当前目录下所有以Nishang_开头的文件夹
    current_dir = os.getcwd()
    print(f"\nSearching for directories starting with 'Nishang_' in: {current_dir}\n")

    # 获取所有以Nishang_开头的子目录
    nishang_dirs = [
        d
        for d in os.listdir(current_dir)
        if os.path.isdir(os.path.join(current_dir, d)) and d.startswith("Nishang_")
    ]

    if not nishang_dirs:
        print("No directories starting with 'Nishang_' found\n")
    else:
        for dir_name in nishang_dirs:
            dir_path = os.path.join(current_dir, dir_name)
            print(f"Processing directory: {dir_path}")
            recursive_make_clean(dir_path)
            print()  # 增加空行分隔不同目录的输出

    os.system("cd ni_shang_manage && cargo clean")

    print("Process completed")
