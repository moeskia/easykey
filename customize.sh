#!/system/bin/sh

wait_volume_key() {
    VOLKEY_RESULT="up"
    while true; do
        if [ -n "$_VOL_DEV" ]; then
            _ev=$(getevent -lqc 1 "$_VOL_DEV" 2>/dev/null)
        else
            _ev=$(getevent -lqc 1 2>/dev/null)
        fi
        case "$_ev" in
            *KEY_VOLUMEUP*DOWN*)   VOLKEY_RESULT="up";    return ;;
            *KEY_VOLUMEDOWN*DOWN*) VOLKEY_RESULT="down";  return ;;
            *KEY_POWER*DOWN*)      input keyevent KEY_POWER 2>/dev/null; VOLKEY_RESULT="power"; return ;;
        esac
    done
}

# 目标型号列表
SUPPORTED_MODELS="plk110 plz110 cph2747 pkx110 plq110 plr110 plc110 pmb110"

# 获取当前设备型号并转为小写
CURRENT_MODEL=$(getprop ro.product.model | tr '[:upper:]' '[:lower:]')
MATCH_FOUND=false

check(){

ui_print "- 正在验证设备型号..."

# 检查当前型号是否在支持列表中
for model in $SUPPORTED_MODELS; do
    if [ "$CURRENT_MODEL" = "$model" ]; then
        MATCH_FOUND=true
        TARGET_CONF_NAME="$model"
        break
    fi
done

# 如果型号不匹配，终止安装并返回 1
if [ "$MATCH_FOUND" = false ]; then
    ui_print "**************************************"
    ui_print " ERROR: 不支持的设备型号! "
    ui_print " 当前设备: $CURRENT_MODEL"
    ui_print " 支持列表: $SUPPORTED_MODELS"
    ui_print "**************************************"
    abort "- 安装已取消：机型不匹配"
fi

ui_print "- 检测到匹配型号: $TARGET_CONF_NAME"

ui_print " "
ui_print "- 请仔细阅读下列模块说明"
ui_print " "

ui_print "- 1.任何模块均有影响系统稳定性乃至损坏设备的可能"
ui_print "- 2.模块作者不对刷写此模块带来的任何后果负责"
ui_print "- 3.使用前，需在系统设置中将快捷键设置为无，否则可能会与模块冲突"
ui_print "- 4.模块配置文件为/data/adb/modules/Easy_Key/config.ini，命令仓库为/data/adb/modules/Easy_Key/repo.json"

ui_print " "
ui_print "- [音量＋]我已仔细阅读并知悉上述说明"
ui_print "- [音量－]退出安装"
ui_print " "
wait_volume_key
if [ "$VOLKEY_RESULT" = "down" ]; then
    abort "- 退出安装"
fi
ui_print "- 已确认阅读说明，继续安装"
ui_print " "

if [ -d "/data/adb/modules/Easy_Key" ]; then
    ui_print "- 检测到 EasyKey 模块正在更新"
    if [ -f "$MODPATH/config.ini" ]; then
        rm -f "$MODPATH/config.ini"
        ui_print "- 已保留原本的快捷键配置"
    fi
    if [ -f "/data/adb/modules/Easy_Key/action.sh" ]; then
        rm -f "/data/adb/modules/Easy_Key/action.sh"
        ui_print "- 已清除不再需要的旧版切换器"
    fi
fi

}

check
