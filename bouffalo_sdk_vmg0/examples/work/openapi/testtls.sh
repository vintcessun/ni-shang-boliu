#!/bin/bash

sites=(
  "https://www.baidu.com/"
  "https://www.bilibili.com/"
  "https://www.qq.com/"
  "https://www.taobao.com/"
  "https://www.jd.com/"
  "https://www.aliyun.com/"
  "https://www.sina.com.cn/"
  "https://www.zhihu.com/"
)

for url in "${sites[@]}"; do
  echo "Testing: $url"
  time_ms=$(curl -o /dev/null -s -w "%{time_total}\n" "$url")
  echo "Total elapsed time: $(awk "BEGIN{printf \"%.0f\", $time_ms*1000}") ms"
  echo "---------------------------------------------"
done