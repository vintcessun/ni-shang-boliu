#!/bin/bash

# --- 配置 ---
SERIAL_PORT="/dev/ttyACM0" # 指定串口设备文件路径

# --- 脚本逻辑 ---
echo "-----------------------------------------------------"
echo " AUTO BUILD & FLASH SCRIPT"
echo "-----------------------------------------------------"
echo "Target Port: ${SERIAL_PORT}"
echo ""

# 1. 关闭占用串口的 minicom (或其他终端程序)
echo "[Step 1/3] 检查并尝试关闭占用 ${SERIAL_PORT} 的终端程序..."

# 使用 fuser 查找并终止占用串口的进程 (通常在 psmisc 包中)
# 不需要 sudo，因为用户已在相关用户组
fuser -k -s ${SERIAL_PORT} 2>/dev/null # -k kill, -s silent, 忽略错误输出

# 检查 fuser 是否成功执行（即使没找到进程，退出码也可能非0，所以这里只是提示）
if [ $? -eq 0 ]; then
    echo "--> 已尝试关闭 ${SERIAL_PORT} 的占用进程 (如果存在)。"
    sleep 0.5 # 短暂等待，确保串口释放
else
    # 如果 fuser 失败 (例如命令不存在或无权)，或者串口本来就没被占用，会进这里
    echo "--> ${SERIAL_PORT} 可能已空闲，或 fuser 执行失败/无权限。"
    # 可以尝试 pkill 作为备选 (如果确认 minicom 命令行包含设备路径)
    # pkill -f "minicom.*${SERIAL_PORT}" 2>/dev/null
fi
echo ""

# 2. 执行编译
echo "[Step 2/3] 编译工程 (make)..."
make "$@"          # 将传递给此脚本的所有参数 (例如 -j8) 传递给 make
BUILD_EXIT_CODE=$? # 获取 make 命令的退出状态码

if [ ${BUILD_EXIT_CODE} -ne 0 ]; then
    echo ""
    echo "[错误] 编译失败，退出码: ${BUILD_EXIT_CODE}. 终止脚本。" >&2 # 输出到标准错误
    exit ${BUILD_EXIT_CODE}
fi
echo "--> 编译成功。"
echo ""

# 3. 执行烧录
echo "[Step 3/3] 烧录固件 (make flash)..."

# 假设 make flash 命令接受 COMX 参数，并且能处理完整的设备路径
# 将传递给此脚本的所有参数 (例如 ERASE=1) 也传递给 make flash
make flash COMX="${SERIAL_PORT}" "$@"
FLASH_EXIT_CODE=$? # 获取 make flash 命令的退出状态码

if [ ${FLASH_EXIT_CODE} -ne 0 ]; then
    echo ""
    echo "[错误] 烧录失败，退出码: ${FLASH_EXIT_CODE}." >&2
    exit ${FLASH_EXIT_CODE}
fi
echo ""
echo "--> 烧录成功!"
echo "-----------------------------------------------------"

# 可选：烧录成功后自动重新打开 minicom
echo "正在重新启动 minicom..."
putty -serial ${SERIAL_PORT} -sercfg 2000000 -geometry 1280x800 &

exit 0
