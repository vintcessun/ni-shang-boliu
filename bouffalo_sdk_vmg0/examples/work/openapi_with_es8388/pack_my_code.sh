#!/bin/bash

# ==============================================================================
#  AISTUDIO 代码同步打包脚本
#  作者: 你牛逼的AI助手 (由你指导)
#  功能: 将项目核心代码打包成一个单一的txt文件，方便与AI进行同步。
# ==============================================================================

# 定义输出文件名
OUTPUT_FILE="code_sync.txt"

# 定义需要打包的文件列表 (根据你的 tree -h 输出，我们只选择最核心的)
FILES_TO_PACK=(
    "src/main.c"
    "modules/unified_gateway_client/include/unified_gateway_client.h"
    "modules/unified_gateway_client/src/unified_gateway_client.c"
    "modules/audio_es8388/include/es8388_driver.h"
    "modules/audio_es8388/src/es8388_driver.c"
    "modules/cli_cmds/src/my_cli_cmds.c"
    "/home/cagedbird/Projects/asr_go_service/main.go"
    "/home/cagedbird/Projects/asr_go_service/tts_service.go"
    "/home/cagedbird/Projects/langchain_bl618/src/api/server.py"
)

# 开始打包前，先清空旧的输出文件
echo "--- START OF AISTUDIO CODE SYNC BUNDLE ---" >"$OUTPUT_FILE"
echo "--- Generated on $(date) ---" >>"$OUTPUT_FILE"

# 循环遍历文件列表
for FILE_PATH in "${FILES_TO_PACK[@]}"; do
    # 检查文件是否存在
    if [ -f "$FILE_PATH" ]; then
        echo "" >>"$OUTPUT_FILE"
        echo "--- START OF FILE $FILE_PATH ---" >>"$OUTPUT_FILE"
        echo "" >>"$OUTPUT_FILE"

        # 将文件内容追加到输出文件
        cat "$FILE_PATH" >>"$OUTPUT_FILE"

        echo "" >>"$OUTPUT_FILE"
        echo "--- END OF FILE $FILE_PATH ---" >>"$OUTPUT_FILE"
        echo "" >>"$OUTPUT_FILE"
        echo "Packed: $FILE_PATH"
    else
        echo "Warning: File not found, skipping: $FILE_PATH"
    fi
done

echo "--- END OF AISTUDIO CODE SYNC BUNDLE ---" >>"$OUTPUT_FILE"

echo ""
echo "=========================================================="
echo "  代码打包完成! "
echo "  请将 '$OUTPUT_FILE' 文件的全部内容发送给AI。"
echo "=========================================================="
