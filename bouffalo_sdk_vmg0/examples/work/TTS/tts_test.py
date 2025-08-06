import logging
from datetime import datetime, timedelta
from logging.handlers import RotatingFileHandler
import base64
from uuid import uuid4

import requests
from fastapi import FastAPI, Request
from fastapi.responses import Response

from contextlib import asynccontextmanager
from typing import Any, Dict

import pyaudio
import struct


# 配置日志
def setup_logging():
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)

    # 控制台日志
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)

    file_handler = RotatingFileHandler(
        "tts_service.log", maxBytes=0, backupCount=3, encoding="utf-8"
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
    logging.info("服务启动中")
    yield
    logging.info("服务关闭中...")


app = FastAPI(title="华为TTS语音合成API", lifespan=lifespan)

# 华为云TTS服务配置
HUAWEI_REGION = "cn-north-4"  # 服务区域
HUAWEI_PROJECT_ID = "88d548d900c3427bb3a60e13674e8178"  # 项目ID
HUAWEI_TTS_ENDPOINT = (
    f"https://sis-ext.{HUAWEI_REGION}.myhuaweicloud.com/v1/{HUAWEI_PROJECT_ID}/tts"
)
HUAWEI_IAM_ENDPOINT = "https://iam.cn-north-4.myhuaweicloud.com/v3/auth/tokens"
HUAWEI_USERNAME = "vinces"  # 华为云账号用户名
HUAWEI_PASSWORD = "lNS1$gbjQSrHfvAEyCf_%teI"  # 华为云账号密码
HUAWEI_DOMAIN = "vintcessun"  # 华为云账号所属域

# Token相关
huawei_token = None
token_expire_time = None

# 音频数据缓存
audio_cache: Dict[str, bytes] = {}


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


def play_base64_pcm(
    base64_data: str, sample_rate: int = 16000, sample_width: int = 2, channels: int = 1
):
    """
    播放base64编码的PCM音频数据

    参数:
    base64_data: base64编码的PCM音频字符串
    sample_rate: 采样率，默认为16000Hz
    sample_width: 采样宽度（字节），默认为2字节(16位)
    channels: 声道数，默认为1(单声道)
    """
    try:
        # 解码base64数据为字节流
        pcm_data = base64.b64decode(base64_data)

        # 初始化PyAudio
        p = pyaudio.PyAudio()

        # 打开音频输出流
        stream = p.open(
            format=p.get_format_from_width(sample_width),
            channels=channels,
            rate=sample_rate,
            output=True,
        )

        # 播放音频
        print("开始播放音频...")
        stream.write(pcm_data)
        print("音频播放完成")

        # 关闭流和PyAudio
        stream.stop_stream()
        stream.close()
        p.terminate()

    except Exception as e:
        print(f"播放音频时出错: {str(e)}")


def mono_to_stereo(mono_pcm_data: bytes):
    """
    将单声道16kHz 16bit的PCM数据转换为双声道

    参数:
    mono_pcm_data: 单声道PCM数据，二进制格式

    返回:
    stereo_pcm_data: 双声道PCM数据，二进制格式
    """
    # 检查数据长度是否为2的倍数（16bit = 2字节）
    if len(mono_pcm_data) % 2 != 0:
        raise ValueError("输入的PCM数据长度必须是2的倍数（16bit采样）")

    stereo_data = bytearray()

    # 每次读取2字节（16bit）的单声道采样
    for i in range(0, len(mono_pcm_data), 2):
        # 解析单声道采样（16bit，小端格式）
        sample = struct.unpack("<h", mono_pcm_data[i : i + 2])[0]

        # 将同一个采样写入左右声道（双声道）
        # 同样使用16bit，小端格式
        stereo_data.extend(struct.pack("<hh", sample, sample))

    return bytes(stereo_data)


@app.get("/api/tts", response_model=None)
async def text_to_speech(request: Request) -> Response:
    """文本转语音接口"""
    try:
        text = request.query_params.get("data", "")
        trace_id = request.query_params.get("trace_id")
        chunk_id = request.query_params.get("chunk_id")
        chunk_size = int(request.query_params.get("chunk_size", 32000))  # 默认1KB

        # 如果是首次请求(只有data参数)
        if text and not trace_id and not chunk_id:
            token = await get_huawei_token()

            # 构造TTS请求体
            tts_body: Dict[str, Any] = {
                "text": text,
                "config": {
                    "audio_format": "pcm",  # PCM格式
                    "sample_rate": "16000",  # 16kHz采样率
                    "property": "chinese_xiaoyan_common",  # 默认发音人
                },
            }

            headers = {"Content-Type": "application/json", "X-Auth-Token": token}

            # 调用华为TTS API
            response = requests.post(
                HUAWEI_TTS_ENDPOINT, json=tts_body, headers=headers
            )
            response.raise_for_status()

            result = response.json()
            if "result" not in result or "data" not in result["result"]:
                raise ValueError("无效的TTS响应格式")

            # 解码Base64音频数据
            src_data = base64.b64decode(result["result"]["data"])
            # play_base64_pcm(result["result"]["data"])
            audio_data = mono_to_stereo(src_data)

            # 生成trace_id并缓存音频数据
            trace_id = str(uuid4())
            audio_cache[trace_id] = audio_data
            return Response(content=trace_id)

        # 如果是后续请求(有trace_id和chunk_id)
        elif trace_id and chunk_id is not None:
            if trace_id not in audio_cache:
                return Response(content="")

            audio_data: bytes = audio_cache[trace_id]
            chunk_id = int(chunk_id)
            start = chunk_id * chunk_size
            end = start + chunk_size
            chunk_data: bytes = audio_data[start:end]
            if len(chunk_data) == 0:
                del audio_cache[trace_id]

            # 将数据块编码为base64
            encoded_chunk = base64.b64encode(chunk_data).decode("utf-8")
            logging.info(f"发送音频数据块 {chunk_id}, 大小: {len(chunk_data)}字节")
            return Response(
                content=encoded_chunk,
            )

        else:
            return Response(content="参数错误: 需要data参数或trace_id+chunk_id参数")

    except Exception as e:
        logging.error(f"语音合成失败: {str(e)}")
        return Response(content=str(e))


if __name__ == "__main__":
    import uvicorn

    logging.info("🚀 启动华为TTS语音合成API服务...")
    uvicorn.run(app, host="0.0.0.0", port=80)
