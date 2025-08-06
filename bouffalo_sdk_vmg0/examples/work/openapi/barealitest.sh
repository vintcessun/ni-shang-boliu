#!/bin/bash

API_KEY="sk-1209977f760d43e3afd3c65dad05b165"
API_URL="https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"
MODEL="llama3.3-70b-instruct"

USER_MSG="你好，测试一下响应速度"

POST_DATA='{
    "model": "'"$MODEL"'",
    "stream": true,
    "messages": [
        {"role": "user", "content": "'"$USER_MSG"'"}
    ]
}'

echo "User message: $USER_MSG"

start=$(date +%s.%3N)

curl -s \
    -H "Authorization: Bearer $API_KEY" \
    -H "Content-Type: application/json" \
    -d "$POST_DATA" \
    "$API_URL" | while read -r line; do
        # 只输出有内容的流式响应
        [[ -n "$line" ]] && echo "$line"
done

end=$(date +%s.%3N)
elapsed=$(awk "BEGIN{printf \"%.0f\", ($end-$start)*1000}")

echo "Total elapsed time: ${elapsed} ms"