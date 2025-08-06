#!/bin/bash

# ==============================================================================
#  项目核心代码打包脚本 (Opus模块定制版)
#  功能: 将用于集成Opus编码模块的核心文件打包，方便AI进行分析和代码编写。
# ==============================================================================

# --- 配置区 ---

# 定义输出文件名
OUTPUT_FILE="freertos_opus_project_bundle.txt"

# 定义需要打包的文件列表 (请根据您项目的实际情况调整文件名)
# 我猜测了一些常见的文件名，比如 main.c, cli_cmds.c, es8388.h
# 如果您的文件名不同，请直接修改这里的列表。
FILES_TO_PACK=(
    "proj.conf"        # 项目组件配置文件
    "CMakeLists.txt"   # 项目构建规则
    "main.c"           # 主入口和任务创建
    "FreeRTOSConfig.h" # FreeRTOS 内核配置

    # --- 请在这里修改成您自己的文件名 ---
    "modules/cli_cmds/src/my_cli_cmds.c"           # 假设这是您注册Shell命令的文件
    "modules/audio_es8388/include/es8388_driver.h" # 假设这是您ES8388驱动的头文件
    "modules/audio_es8388/src/es8388_driver.c"     # 假设这是您ES8388驱动的源文件
    # ------------------------------------
)

# --- 执行区 ---

# 开始打包前，先清空旧的输出文件
echo "--- START OF PROJECT CODE BUNDLE (for Opus Module) ---" >"$OUTPUT_FILE"
echo "--- Generated on $(date) ---" >>"$OUTPUT_FILE"

# 循环遍历文件列表
for FILE_PATH in "${FILES_TO_PACK[@]}"; do
    # 检查文件是否存在
    if [ -f "$FILE_PATH" ]; then
        echo "" >>"$OUTPUT_FILE"
        echo "================================================================================" >>"$OUTPUT_FILE"
        echo "--- START OF FILE: $FILE_PATH ---" >>"$OUTPUT_FILE"
        echo "================================================================================" >>"$OUTPUT_FILE"
        echo "" >>"$OUTPUT_FILE"

        # 将文件内容追加到输出文件
        cat "$FILE_PATH" >>"$OUTPUT_FILE"

        echo "" >>"$OUTPUT_FILE"
        echo "--- END OF FILE: $FILE_PATH ---" >>"$OUTPUT_FILE"
        echo "" >>"$OUTPUT_FILE"
        echo "✅ Packed: $FILE_PATH"
    else
        # 特别注意：如果找不到文件，只发出警告，但继续执行
        echo "⚠️  Warning: File not found, skipping: $FILE_PATH"
        echo "    (If this file is important, please edit the script and add the correct path)"
    fi
done

echo "--- END OF PROJECT CODE BUNDLE (for Opus Module) ---" >>"$OUTPUT_FILE"

echo ""
echo "=========================================================="
echo "  🚀 代码打包完成! "
echo "  请将 '$OUTPUT_FILE' 文件的全部内容发送给我。"
echo "=========================================================="
