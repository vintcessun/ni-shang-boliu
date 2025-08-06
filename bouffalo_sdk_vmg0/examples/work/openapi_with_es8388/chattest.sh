#!/bin/bash

# 你要测试的 URL 和 POST 数据
URL="http://39.104.209.130:8000/chat"
POST_DATA='{"user_message":"hello!","session_id":"testkey1"}'
HEADER="X-API-Key: testkey1"
CONTENT_TYPE="application/json"

# 记录开始时间（单位：秒.微秒）
start=$(date +%s.%3N)

# 发起 POST 请求，-s 静默，-o /dev/null 不输出正文，-w 输出响应码
curl -s -o /dev/null -w "HTTP Status: %{http_code}\n" \
     -H "$HEADER" -H "Content-Type: $CONTENT_TYPE" \
     -d "$POST_DATA" "$URL"

# 记录结束时间
end=$(date +%s.%3N)

# 计算耗时（毫秒）
elapsed=$(awk "BEGIN{printf \"%.0f\", ($end-$start)*1000}")

echo "Total elapsed time: ${elapsed} ms"