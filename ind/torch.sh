#!/system/bin/sh

# 这里的 NODE 选择了 white:flash-1，这是最常用的白光手电筒
# 如果你想让光线偏黄，可以改成 yellow:flash-0
NODE="/sys/class/leds/white:flash-1/brightness"

# 检查节点是否存在
if [ ! -f "$NODE" ]; then
    exit 1
fi

# 读取当前亮度
CURRENT=$(cat "$NODE")

if [ "$CURRENT" -eq 0 ]; then
    # 开启：写入最大亮度 255
    echo 255 > "$NODE"
else
    # 关闭：写入 0
    echo 0 > "$NODE"
fi
