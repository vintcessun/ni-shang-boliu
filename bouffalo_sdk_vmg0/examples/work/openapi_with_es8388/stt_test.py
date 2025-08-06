import asyncio
import base64
import json
import uuid
import hashlib
import hmac
import logging
from typing import Dict, Optional, Any
from datetime import datetime, timedelta
from logging.handlers import RotatingFileHandler

import requests
import websockets
from fastapi import FastAPI, WebSocket, HTTPException, Body,Request
from starlette.websockets import WebSocketDisconnect

from contextlib import asynccontextmanager


# 配置日志
def setup_logging():
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)

    # 控制台日志
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)

    file_handler = RotatingFileHandler(
        "stt_service.log", maxBytes=0, backupCount=3, encoding="utf-8"
    )
    file_handler.setLevel(logging.INFO)

    # 日志格式
    formatter = logging.Formatter(
        "%(asctime)s - %(levelname)s - %(message)s", datefmt="%Y-%m-%d %H:%M:%S"
    )
    console_handler.setFormatter(formatter)
    file_handler.setFormatter(formatter)

    logger.addHandler(console_handler)
    logger.addHandler(file_handler)


# 初始化日志
setup_logging()


@asynccontextmanager
async def lifespan(app: FastAPI):
    """处理FastAPI生命周期事件"""
    logging.info("服务启动中，初始化STT服务连接...")
    # await initialize_stt_connection()
    yield
    logging.info("服务关闭中...")
    # 发送结束消息
    end_msg = '{"command": "END", "cancel": "false"}'
    if stt_websocket is not None:
        await stt_websocket.send(end_msg)
        logging.info(f"📤 已发送结束识别指令")
    else:
        logging.warning(f"⚠️ 发送结束指令失败，STT连接已关闭")


app = FastAPI(title="华为STT语音识别API", lifespan=lifespan)

# 华为云STT服务配置 - 请替换为你的实际配置
HUAWEI_REGION = "cn-north-4"  # 服务区域，例如cn-north-4
HUAWEI_PROJECT_ID = "88d548d900c3427bb3a60e13674e8178"  # 项目ID
HUAWEI_IAM_ENDPOINT = "https://iam.cn-north-4.myhuaweicloud.com/v3/auth/tokens"
HUAWEI_USERNAME = "vinces"  # 华为云账号用户名
HUAWEI_PASSWORD = "lNS1$gbjQSrHfvAEyCf_%teI"  # 华为云账号密码
HUAWEI_DOMAIN = "vintcessun"  # 华为云账号所属域

# Token相关
huawei_token = None
token_expire_time = None

# 全局变量：维护STT服务连接和流信息
stt_websocket: Optional[Any] = None  # type: ignore
stt_initialized: bool = False  # type: ignore  # 标记WSS连接是否已初始化
stt_lock = asyncio.Lock()  # 确保初始化过程线程安全
active_streams: Dict[int, str] = {}  # 存储活跃的流ID和对应的request_id
stream_counter = 0  # 用于跟踪流的总数


def sign_string(key: str, source: str) -> str:
    """使用HMAC-SHA256进行签名"""
    hmac_obj = hmac.new(key.encode("utf-8"), source.encode("utf-8"), hashlib.sha256)
    return base64.b64encode(hmac_obj.digest()).decode("utf-8")


async def get_huawei_token():
    """获取华为云IAM Token"""
    global huawei_token, token_expire_time

    # 如果Token未过期，直接返回现有Token
    if huawei_token and token_expire_time and datetime.now() < token_expire_time:
        return huawei_token

    try:
        auth_data: Dict[str, Any] = {
            "auth": {
                "identity": {
                    "password": {
                        "user": {
                            "name": HUAWEI_USERNAME,
                            "password": HUAWEI_PASSWORD,
                            "domain": {"name": HUAWEI_DOMAIN},
                        }
                    },
                    "methods": ["password"],
                },
                "scope": {"project": {"name": HUAWEI_REGION}},
            }
        }

        response = requests.post(HUAWEI_IAM_ENDPOINT, json=auth_data)

        response.raise_for_status()

        # 从响应头获取Token
        huawei_token = response.headers.get("X-Subject-Token")
        if not huawei_token:
            raise ValueError("获取Token失败，响应中未找到X-Subject-Token")

        # 计算Token过期时间（默认24小时有效期）
        token_expire_time = datetime.now() + timedelta(hours=23)
        logging.info("✅ 成功获取华为云Token")
        return huawei_token

    except Exception as e:
        logging.error(f"❌ 获取华为云Token失败: {str(e)}")
        raise


def get_stt_websocket_url():
    """
    根据华为云官方文档获取STT服务的WebSocket连接地址
    参考文档: https://support.huaweicloud.com/api-sis/sis_03_0027.html
    """
    host = f"sis-ext.{HUAWEI_REGION}.myhuaweicloud.com"  # 修正为正确的域名
    url = f"wss://{host}/v1/{HUAWEI_PROJECT_ID}/rasr/continue-stream"
    logging.info(f"使用WebSocket地址: {url}")
    return url


async def initialize_stt_connection():
    """初始化与华为STT服务的WebSocket连接（单例模式，只初始化一次）"""
    global stt_websocket, stt_initialized

    # 使用锁确保并发情况下只初始化一次
    async with stt_lock:
        if stt_initialized and stt_websocket:
            logging.info("STT服务连接已初始化，直接使用现有连接")
            return

        try:
            logging.info("开始初始化STT服务连接...")

            # 获取华为云Token
            token = await get_huawei_token()
            if not token:
                raise Exception("无法获取有效的华为云Token")

            websocket_url = get_stt_websocket_url()
            if not websocket_url:
                raise Exception("无法获取STT服务地址")

            headers = {"X-Auth-Token": token}

            # 根据华为文档调整连接参数
            stt_websocket = await websockets.connect(
                websocket_url,
                additional_headers=headers,
                open_timeout=None,
                ping_timeout=None,
                close_timeout=None,
            )
            stt_initialized = True
            logging.info("✅ 已成功连接到华为STT服务，连接将被所有请求共享")

            # 发送开始消息
            start_msg: Dict[str, Any] = {
                "command": "START",
                "config": {
                    "audio_format": "pcm16k16bit",
                    "property": "chinese_16k_general",
                    "add_punc": "yes",
                    "digit_norm": "yes",
                    "interim_results": "no",
                    "need_word_info": "no",
                },
            }
            await stt_websocket.send(json.dumps(start_msg))
            logging.info("📤 已发送开始识别指令")

            # 启动一个独立的任务来接收所有STT服务的响应
            async def receive_all_stt_responses():
                global stt_websocket, stt_initialized
                retry_count = 0
                max_retries = 5
                base_delay = 1  # 初始延迟1秒

                while True:
                    try:
                        if not stt_websocket or not stt_initialized:
                            logging.info("尝试重新连接STT服务...")
                            try:
                                # 关闭现有连接
                                if stt_websocket:
                                    await stt_websocket.close()

                                # 重新初始化连接
                                await initialize_stt_connection()
                                retry_count = 0  # 重置重试计数
                                continue
                            except Exception as e:
                                retry_count += 1
                                if retry_count >= max_retries:
                                    logging.error(
                                        f"❌ 达到最大重试次数({max_retries})，停止重连"
                                    )
                                    stt_initialized = False
                                    break

                                # 指数退避
                                delay = min(
                                    base_delay * (2 ** (retry_count - 1)), 30
                                )  # 最大30秒
                                logging.info(f"等待{delay}秒后重试...")
                                await asyncio.sleep(delay)
                                continue

                        # 接收STT服务响应
                        response = await stt_websocket.recv()
                        result = json.loads(response)

                        # 记录完整的原始响应
                        timestamp = datetime.now().strftime("%H:%M:%S")
                        logging.info(f"\n[{timestamp}] 📥 收到STT服务完整响应:")
                        logging.info(json.dumps(result, ensure_ascii=False, indent=2))

                        # 提取request_id以识别对应的流
                        request_id = result.get("header", {}).get("request_id")
                        if not request_id:
                            continue

                        # 查找对应的stream_id
                        stream_id = None
                        for sid, rid in active_streams.items():
                            if rid == request_id:
                                stream_id = sid
                                break

                        if stream_id is None:
                            continue

                        # 处理识别结果
                        if "result" in result and "text" in result["result"]:
                            recognized_text = result["result"]["text"]
                            logging.info(
                                f"[{timestamp}] 🎤 流 #{stream_id} 识别文本: {recognized_text}"
                            )

                        # 检查是否识别结束
                        if "header" in result and result["header"].get("status") == 2:
                            logging.info(f"[{timestamp}] ✅ 流 #{stream_id} 识别已完成")
                            # 从活跃流中移除
                            if stream_id in active_streams:
                                del active_streams[stream_id]

                    except websockets.exceptions.ConnectionClosed:
                        logging.error("❌ 与STT服务的连接已关闭")
                        stt_initialized = False
                        stt_websocket = None
                    except Exception as e:
                        logging.error(f"❌ 处理STT响应时出错: {str(e)}")
                        stt_initialized = False
                        stt_websocket = None

            asyncio.create_task(receive_all_stt_responses())
        except Exception as e:
            logging.error(f"❌ 初始化STT连接失败: {str(e)}")
            stt_initialized = False
            raise


def convert_stereo_to_mono(audio_data: bytes) -> bytes:
    """将双声道16k16bit PCM数据转换为单声道"""
    mono_data = bytearray()
    for i in range(0, len(audio_data), 4):  # 每个采样点4字节(左右声道各2字节)
        # 取左右声道的平均值
        left = int.from_bytes(audio_data[i : i + 2], "little", signed=True)
        right = int.from_bytes(audio_data[i + 2 : i + 4], "little", signed=True)
        avg = (left + right) // 2
        mono_data.extend(avg.to_bytes(2, "little", signed=True))
    return bytes(mono_data)


async def handle_audio_data( audio_data: bytes) -> None:
    """处理HTTP请求中的音频数据，通过共享的STT连接进行识别"""
    global stream_counter
    stream_counter += 1
    logging.info(f"\n🚀 开始处理音频数据 (总第{stream_counter}个请求)")

    # 生成唯一标识
    request_id = str(uuid.uuid4())
    logging.info(f"📋 音频数据 请求ID: {request_id}")

    try:
        # 将双声道转为单声道
        audio_data = convert_stereo_to_mono(audio_data)

        # 直接发送音频数据(二进制帧)
        if stt_websocket is not None:
            await stt_websocket.send(audio_data)
        logging.info(f"🔄 音频数据已发送")

    except Exception as e:
        logging.error(f"❌ 处理音频数据时出错: {str(e)}")
        raise


async def handle_audio_stream(stream_id: int, client_websocket: WebSocket) -> None:
    """处理客户端发送的音频流，通过共享的STT连接进行识别"""
    global stream_counter
    stream_counter += 1
    logging.info(f"\n🚀 开始处理流 #{stream_id} (总第{stream_counter}个流)")

    # 生成唯一标识
    request_id = str(uuid.uuid4())
    active_streams[stream_id] = request_id
    logging.info(f"📋 流 #{stream_id} 请求ID: {request_id}")

    try:
        # 创建缓冲区存储音频数据 (500ms数据=16kHz*2声道*2字节*500ms=32000字节)
        audio_buffer = bytearray()

        if stt_websocket is None:
            raise Exception("STT WebSocket连接未建立")

        # 接收客户端的语音数据并发送给STT服务
        audio_chunk_count = 0
        while True:
            data = await client_websocket.receive_bytes()
            audio_chunk_count += 1

            # 每接收10个音频块记录一次状态
            if audio_chunk_count % 10 == 0:
                logging.info(f"🔄 流 #{stream_id} 已接收 {audio_chunk_count} 个音频块")

            # 将数据添加到缓冲区
            audio_buffer.extend(data)

            # 每收集500ms数据后处理一次(16kHz*2声道*2字节*500ms=32000字节)
            if len(audio_buffer) >= 32000:
                # 转换为单声道(16kHz*2字节*500ms=16000字节)
                mono_data = convert_stereo_to_mono(bytes(audio_buffer))

                # 检查分片大小是否符合要求(16k音频要求320-65536字节)
                if len(mono_data) < 320 or len(mono_data) > 65536:
                    raise Exception(f"无效的分片大小: {len(mono_data)}字节")

                # 发送处理后的数据
                await stt_websocket.send(mono_data)
                logging.info(f"📤 发送 {len(mono_data)}字节音频数据")
                audio_buffer.clear()

    except WebSocketDisconnect:
        logging.info(f"🔌 客户端断开连接，流 #{stream_id}")
    except Exception as e:
        logging.error(f"❌ 流 #{stream_id} 处理音频时出错: {str(e)}")
    finally:
        # 发送结束消息
        end_msg = '{"command": "END", "cancel": "false"}'
        if stt_websocket is not None:
            await stt_websocket.send(end_msg)
            logging.info(f"📤 流 #{stream_id} 已发送结束识别指令")
        else:
            logging.warning(f"⚠️ 流 #{stream_id} 发送结束指令失败，STT连接已关闭")


@app.post("/api/stt")
async def asr_stream(
    audio_data: bytes = Body(...),
) -> Dict[str, Any]:
    """语音识别HTTP接口，使用共享连接处理语音数据

    Args:
        audio_data: 二进制音频数据，格式应为PCM 16kHz 16bit
    """
    global stt_initialized
    timestamp = datetime.now().strftime("%H:%M:%S")
    logging.info(f"\n[{timestamp}] 🔗 新的语音识别请求")

    try:
        # 检查STT连接状态，如已关闭则重新初始化
        if not stt_initialized or not stt_websocket:
            logging.warning("⚠️ STT连接未初始化或已关闭，重新初始化连接...")
            await initialize_stt_connection()

        # 处理语音数据
        await handle_audio_data(audio_data)
        return {"status": "success"}

    except WebSocketDisconnect:
        logging.info(f"🔌 客户端断开连接")
        stt_initialized = False
        return {"status": "error"}
    except Exception as e:
        error_msg = f"处理语音数据 时发生错误: {str(e)}"
        logging.info(f"❌ {error_msg}")
        raise HTTPException(status_code=500, detail=error_msg)

@app.post("/api/to_rust_stt")
async def send_to_rust(
    request:Request,
    audio_data: bytes = Body(...),
)->Any:
    try:
        received_headers = dict(request.headers)
        logging.info(f"收到的请求头: {received_headers}")

        # 定义目标STT服务的URL
        stt_url = "http://127.0.0.1/api/stt"
        
        # 在异步函数中运行同步的requests调用
        loop = asyncio.get_event_loop()
        response = await loop.run_in_executor(
            None, 
            # 发送POST请求，将音频数据作为二进制内容发送
            lambda: requests.post(
                stt_url,
                data=audio_data,
                headers={"Content-Type": "application/octet-stream"}
            )
        )
        
        # 检查请求是否成功
        response.raise_for_status()
        
        # 返回STT服务的响应结果
        return {
            "status": "success",
            "stt_response": response.json()  # 假设STT服务返回JSON响应
        }
        
    except requests.exceptions.RequestException as e:
        # 处理请求相关的异常
        return {
            "status": "error",
            "message": f"Failed to send data to STT service: {str(e)}"
        }
    except Exception as e:
        # 处理其他异常
        return {
            "status": "error",
            "message": f"An unexpected error occurred: {str(e)}"
        }


if __name__ == "__main__":
    import uvicorn

    logging.info("🚀 启动华为STT语音识别API服务...")
    uvicorn.run(app, host="0.0.0.0", port=8000)
