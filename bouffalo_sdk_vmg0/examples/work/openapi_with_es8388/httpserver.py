import asyncio
from aiohttp import web
import os
import aiofiles
import hashlib
import uuid
from datetime import datetime
import struct
from typing import Dict, Any, Optional, Tuple

# --- 配置 ---
HOST_NAME = "0.0.0.0"
PORT_NUMBER = 8000
BASE_UPLOAD_PATH = "/api/v1/asr/stream"  # 基础路径
OUTPUT_FILE = "received_audio.pcm"
TOTAL_CHUNKS = 1000  # 0到999共1000个块
LOG_FILE = "stream_server.log"  # 详细日志文件
MAX_OVERLAP_CHECK = 2048  # 最大检查重叠字节数
PCM_FRAME_SIZE = 4  # 16位双声道PCM的帧大小(2字节/声道 × 2声道)
SILENCE_THRESHOLD = 1000  # 静音阈值（绝对值，对于16位PCM，0-32767）
SILENCE_FRAME_COUNT = 5  # 连续多少帧视为静音区域
# ------------


class ASRStreamHandler:
    def __init__(self) -> None:
        # 跟踪已接收的块及其元数据
        self.received_chunks: Dict[int, Dict[str, Any]] = (
            {}
        )  # {块编号: {version, data, timestamp, request_id, hash}}
        self.lock: asyncio.Lock = asyncio.Lock()
        self.estimated_chunk_size: Optional[int] = None
        self.request_counter: int = 0  # 全局请求计数器
        # 支持长连接的相关配置
        self.keep_alive_timeout: int = 30  # 长连接超时时间(秒)
        # 记录文件当前有效长度（不包括静音）
        self.current_file_length: int = 0

    async def log(self, message: str) -> None:
        """写入日志到文件和控制台"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        log_entry = f"[{timestamp}] {message}\n"

        # 打印到控制台
        print(log_entry.strip())

        # 写入日志文件
        async with aiofiles.open(LOG_FILE, "a") as f:
            await f.write(log_entry)

    async def init_file(self) -> None:
        """初始化输出文件和日志"""
        # 清空输出文件
        async with aiofiles.open(OUTPUT_FILE, "wb") as f:
            await f.write(b"")

        # 清空日志文件
        async with aiofiles.open(LOG_FILE, "w") as f:
            await f.write("=== 服务器启动 ===\n")

        self.received_chunks.clear()
        self.estimated_chunk_size = None
        self.request_counter = 0
        self.current_file_length = 0
        await self.log("服务器已初始化，准备接收0-999共1000个音频块")
        await self.log(f"适配客户端配置: 接收application/octet-stream格式，支持长连接")
        await self.log(
            f"PCM格式验证: 确保数据为16位双声道格式 (每个帧{PCM_FRAME_SIZE}字节)"
        )
        await self.log(
            f"静音检测: 阈值={SILENCE_THRESHOLD}, 连续{SILENCE_FRAME_COUNT}帧视为静音将被跳过"
        )

    def find_overlap(self, prev_data: bytes, curr_data: bytes) -> int:
        """查找两个PCM数据块的最大重叠部分，确保重叠在帧边界上"""
        # 限制检查范围，提高效率
        check_length = min(len(prev_data), len(curr_data), MAX_OVERLAP_CHECK)
        if check_length == 0:
            return 0

        # 从最大可能的重叠长度开始检查，步长为PCM帧大小
        max_possible = check_length - (check_length % PCM_FRAME_SIZE)
        for overlap in range(max_possible, 0, -PCM_FRAME_SIZE):
            # 比较前一个块的尾部和当前块的头部
            if prev_data[-overlap:] == curr_data[:overlap]:
                return overlap
        return 0

    async def ensure_valid_pcm_length(self, data: bytes) -> bytes:
        """确保PCM数据长度是帧大小的倍数，否则截断到最近的有效长度"""
        data_len = len(data)
        remainder = data_len % PCM_FRAME_SIZE
        if remainder != 0:
            truncated = data[: data_len - remainder]
            await self.log(
                f"PCM数据截断: 从{data_len}字节到{len(truncated)}字节 (确保是{PCM_FRAME_SIZE}的倍数)"
            )
            return truncated
        return data

    def is_silent_frame(self, frame: bytes) -> bool:
        """检查单个PCM帧是否为静音帧"""
        # 解析16位双声道帧（小端格式）
        left, right = struct.unpack("<hh", frame)
        # 检查左右声道是否都低于静音阈值
        return abs(left) <= SILENCE_THRESHOLD and abs(right) <= SILENCE_THRESHOLD

    def remove_silence(self, data: bytes) -> Tuple[bytes, int]:
        """从PCM数据中移除静音区域"""
        if len(data) < PCM_FRAME_SIZE:
            return data, 0

        frames: list[bytes] = [
            data[i : i + PCM_FRAME_SIZE] for i in range(0, len(data), PCM_FRAME_SIZE)
        ]
        non_silent_frames: list[bytes] = []
        silent_count = 0
        total_removed = 0

        for frame in frames:
            if self.is_silent_frame(frame):
                silent_count += 1
                # 如果达到静音阈值，开始跳过
                if silent_count >= SILENCE_FRAME_COUNT:
                    total_removed += 1
                    continue
            else:
                # 如果之前有积累的静音帧但未达到阈值，全部保留
                if silent_count > 0:
                    non_silent_frames.extend(frames[-silent_count:])
                    silent_count = 0
                non_silent_frames.append(frame)

        # 处理结尾的静音帧
        if silent_count < SILENCE_FRAME_COUNT and silent_count > 0:
            non_silent_frames.extend(frames[-silent_count:])

        # 合并非静音帧
        non_silent_data = b"".join(non_silent_frames)
        removed_bytes = total_removed * PCM_FRAME_SIZE

        return non_silent_data, removed_bytes

    async def handle_chunk(self, request: web.Request) -> web.Response:
        # 生成唯一请求ID
        request_id = str(uuid.uuid4())[:8]
        self.request_counter += 1
        current_request = self.request_counter

        # 记录请求基本信息
        client_ip = request.remote
        method = request.method
        path = request.path

        # 记录客户端发送的头部信息（用于调试）
        content_type = request.headers.get("Content-Type", "未指定")
        connection = request.headers.get("Connection", "未指定")

        await self.log(
            f"请求 #{current_request} (ID: {request_id}) - "
            f"客户端: {client_ip}, 方法: {method}, 路径: {path}, "
            f"Content-Type: {content_type}, Connection: {connection}"
        )

        # 从URL路径中提取块编号
        try:
            chunk_id = request.match_info["chunk_id"]
            chunk_number = int(chunk_id)
        except (KeyError, ValueError) as e:
            await self.log(f"请求 #{current_request} 错误: {str(e)}")
            return web.Response(text="无效的块编号格式", status=400)

        # 验证块编号范围
        if chunk_number < 0 or chunk_number >= TOTAL_CHUNKS:
            error_msg = f"块编号 {chunk_number} 超出范围 (0-{TOTAL_CHUNKS-1})"
            await self.log(f"请求 #{current_request} 错误: {error_msg}")
            return web.Response(text=error_msg, status=400)

        try:
            # 读取原始二进制流（客户端发送的lan_audio_chunk_buffer）
            data = await request.content.read()
            # 确保数据长度符合PCM格式要求
            data = await self.ensure_valid_pcm_length(data)
            original_size = len(data)

            # 移除静音区域
            data, removed_bytes = self.remove_silence(data)
            chunk_size = len(data)

            # 计算数据哈希，用于验证数据完整性
            data_hash = hashlib.md5(data).hexdigest()[:8]

            await self.log(
                f"请求 #{current_request} - 块 {chunk_number}, "
                f"原始大小: {original_size} bytes, 移除静音后: {chunk_size} bytes "
                f"(跳过 {removed_bytes} bytes 静音), 数据哈希: {data_hash}"
            )

            # 动态更新预估块大小
            if self.estimated_chunk_size is None:
                self.estimated_chunk_size = chunk_size
            else:
                self.estimated_chunk_size = int(
                    (self.estimated_chunk_size * 0.7 + chunk_size * 0.3)
                )

            # 处理块数据
            async with self.lock:
                # 检查是否是重复接收（相同块编号和数据）
                is_duplicate_data = False
                if chunk_number in self.received_chunks:
                    prev_hash = self.received_chunks[chunk_number]["hash"]
                    if prev_hash == data_hash:
                        is_duplicate_data = True

                # 记录块信息（保存原始数据用于重叠检测）
                version = (
                    self.received_chunks.get(chunk_number, {}).get("version", 0) + 1
                )
                self.received_chunks[chunk_number] = {
                    "version": version,
                    "data": data,
                    "timestamp": datetime.now().isoformat(),
                    "request_id": request_id,
                    "hash": data_hash,
                    "size": chunk_size,
                    "original_size": original_size,
                    "removed_silence": removed_bytes,
                }

                # 查找与前一块的重叠
                overlap = 0
                prev_number = chunk_number - 1
                if (
                    prev_number in self.received_chunks
                    and not is_duplicate_data
                    and chunk_size > 0
                ):
                    prev_data = self.received_chunks[prev_number]["data"]
                    overlap = self.find_overlap(prev_data, data)
                    if overlap > 0:
                        await self.log(
                            f"块 {prev_number} 与 {chunk_number} 发现重叠: {overlap} bytes (在帧边界上)"
                        )

                # 计算写入位置和数据
                write_data = data
                if chunk_number == 0:
                    # 第一个块直接写入
                    pass
                else:
                    # 后续块根据重叠情况写入
                    if overlap > 0:
                        # 去除重叠部分
                        write_data = data[overlap:]

                # 写入数据（如果有有效数据）
                write_size = len(write_data)
                if write_size > 0:
                    async with aiofiles.open(OUTPUT_FILE, "ab") as f:
                        await f.write(write_data)
                    self.current_file_length += write_size
                    await self.log(
                        f"块 {chunk_number} 写入 {write_size} bytes (去除重叠和静音后)"
                    )
                else:
                    await self.log(
                        f"块 {chunk_number} 无有效数据可写入 (全部为静音或重叠)"
                    )

                # 记录处理结果
                unique_received = len(self.received_chunks)
                progress = (unique_received / TOTAL_CHUNKS) * 100

                if is_duplicate_data:
                    await self.log(
                        f"请求 #{current_request} - 块 {chunk_number} 重复数据 (版本 {version}), "
                        f"已接收: {unique_received}/{TOTAL_CHUNKS} ({progress:.1f}%)"
                    )
                else:
                    await self.log(
                        f"请求 #{current_request} - 块 {chunk_number} 已处理 (版本 {version}), "
                        f"已接收: {unique_received}/{TOTAL_CHUNKS} ({progress:.1f}%), "
                        f"当前文件总大小: {self.current_file_length} bytes"
                    )

            # 检查是否所有块都已接收
            if unique_received == TOTAL_CHUNKS:
                await self.log(
                    f"所有块已接收，最终文件大小: {self.current_file_length} bytes"
                )
                return web.Response(
                    text="所有块已接收，文件符合PCM格式要求（已去除静音区域）",
                    status=200,
                )

            return web.Response(
                text=f"块 {chunk_number} 已接收 (版本 {version})，移除 {removed_bytes} bytes 静音",
                status=200,
            )

        except Exception as e:
            error_msg = f"处理请求 #{current_request} 时出错: {str(e)}"
            await self.log(error_msg)
            return web.Response(text=error_msg, status=500)


async def main() -> None:
    handler = ASRStreamHandler()
    await handler.init_file()

    app = web.Application()
    # 配置路由，匹配客户端发送的URL格式
    app.router.add_post(f"{BASE_UPLOAD_PATH}/{{chunk_id}}", handler.handle_chunk)

    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, HOST_NAME, PORT_NUMBER)
    await site.start()

    await handler.log(f"ASR流服务器已在 {HOST_NAME}:{PORT_NUMBER} 启动")
    await handler.log(f"接收路径格式: {BASE_UPLOAD_PATH}/[块编号] (0-999)")
    await handler.log(f"输出文件: {OUTPUT_FILE}, 日志文件: {LOG_FILE}")

    await asyncio.Event().wait()


if __name__ == "__main__":
    if os.path.exists(OUTPUT_FILE):
        print(f"警告: 文件 '{OUTPUT_FILE}' 已存在，将被覆盖")

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n服务器正在关闭...")
