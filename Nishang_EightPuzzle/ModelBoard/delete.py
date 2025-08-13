import os
import hashlib
import argparse
from typing import Any, List
from tqdm import tqdm  # 导入tqdm库用于显示进度条


def calculate_multiple_hashes(file_path: str, block_size: int = 65536):
    """计算文件的MD5、SHA-1、SHA-256三种哈希值"""
    md5_hash = hashlib.md5()
    sha1_hash = hashlib.sha1()
    sha256_hash = hashlib.sha256()

    try:
        with open(file_path, "rb") as f:
            # 分块读取，同时更新三种哈希
            for block in iter(lambda: f.read(block_size), b""):
                md5_hash.update(block)
                sha1_hash.update(block)
                sha256_hash.update(block)

        return {
            "md5": md5_hash.hexdigest(),
            "sha1": sha1_hash.hexdigest(),
            "sha256": sha256_hash.hexdigest(),
        }
    except Exception as e:
        print(f"计算文件 {file_path} 的哈希时出错: {e}")
        return None


def find_duplicate_files(folder_path: str) -> dict[Any, Any]:
    """查找三种哈希值完全相同的重复文件"""
    # 键：(md5, sha1, sha256) 元组，值：文件路径列表
    hash_tuple_to_paths: dict[Any, Any] = {}

    # 先收集所有文件路径，用于进度条
    all_files: List[str] = []
    for root, _, files in os.walk(folder_path):
        for file in files:
            file_path = os.path.join(root, file)
            if not os.path.islink(file_path):  # 跳过符号链接
                all_files.append(file_path)

    # 使用tqdm显示进度条
    for file_path in tqdm(all_files, desc="计算文件哈希值", unit="个文件"):
        # 计算三种哈希值
        hashes = calculate_multiple_hashes(file_path)
        if not hashes:
            continue

        # 用三种哈希的元组作为唯一标识
        hash_tuple = (hashes["md5"], hashes["sha1"], hashes["sha256"])

        if hash_tuple in hash_tuple_to_paths:
            hash_tuple_to_paths[hash_tuple].append(file_path)
        else:
            hash_tuple_to_paths[hash_tuple] = [file_path]

    # 只保留有重复的文件组（数量>1）
    return {
        hash_tuple: paths
        for hash_tuple, paths in hash_tuple_to_paths.items()
        if len(paths) > 1
    }


def delete_duplicates(duplicates: dict[Any, Any], dry_run: bool = True):
    """删除重复文件（三种哈希均相同），保留每组中的一个"""
    total_deleted = 0
    total_freed_space = 0

    # 计算总共有多少个文件需要处理删除
    total_files_to_process = sum(len(paths) - 1 for paths in duplicates.values())
    
    # 使用tqdm显示删除进度
    with tqdm(total=total_files_to_process, desc="处理重复文件", unit="个文件") as pbar:
        for hash_tuple, paths in duplicates.items():
            md5, sha1, sha256 = hash_tuple
            print(f"\n哈希值匹配组:")
            print(f"  MD5:    {md5}")
            print(f"  SHA-1:  {sha1}")
            print(f"  SHA-256: {sha256}")
            print(f"  包含 {len(paths)} 个文件:")
            for path in paths:
                print(f"    {path}")

            # 保留第一个文件，标记其余为待删除
            keep = paths[0]
            to_delete = paths[1:]
            print(f"  将保留: {keep}")

            if not dry_run:
                for file_path in to_delete:
                    try:
                        file_size = os.path.getsize(file_path)
                        os.remove(file_path)
                        total_deleted += 1
                        total_freed_space += file_size
                        print(f"  已删除: {file_path}")
                    except Exception as e:
                        print(f"  删除 {file_path} 时出错: {e}")
                    pbar.update(1)  # 更新进度条
            else:
                # 预览模式下也更新进度条
                pbar.update(len(to_delete))

    print(f"\n操作结果:")
    if dry_run:
        print(
            f"  预览模式: 发现 {total_deleted} 个可删除的重复文件，约可释放 {total_freed_space / (1024*1024):.2f} MB 空间"
        )
        print(f"  使用 --delete 选项执行实际删除操作")
    else:
        print(
            f"  已删除 {total_deleted} 个文件，释放约 {total_freed_space / (1024*1024):.2f} MB 空间"
        )


def main():
    parser = argparse.ArgumentParser(
        description="通过多哈希校验查找并删除重复文件（降低误删风险）"
    )
    parser.add_argument(
        "--delete", action="store_true", help="实际执行删除操作，默认仅预览"
    )
    parser.add_argument(
        "folder", nargs='?', default="./dataset", help="要处理的文件夹路径，默认为./dataset"
    )

    args = parser.parse_args()
    folder_path = args.folder

    if not os.path.isdir(folder_path):
        print(f"错误: {folder_path} 不是有效的文件夹")
        return

    print(f"正在扫描 {folder_path} 中的文件（计算MD5、SHA-1、SHA-256哈希）...")
    duplicates = find_duplicate_files(folder_path)

    if not duplicates:
        print("没有找到三种哈希值完全相同的重复文件")
        return

    print(f"找到 {len(duplicates)} 组重复文件（三种哈希值均匹配）")
    delete_duplicates(duplicates, dry_run=not args.delete)


if __name__ == "__main__":
    main()
