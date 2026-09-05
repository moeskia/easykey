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

SUPPORTED_MODELS="plk110 plz110 cph2747 pkx110 plq110 plr110 plc110 pmb110"

CURRENT_MODEL=$(getprop ro.product.model | tr '[:upper:]' '[:lower:]')
MATCH_FOUND=false

check(){

ui_print "- 正在验证设备型号..."

for model in $SUPPORTED_MODELS; do
    if [ "$CURRENT_MODEL" = "$model" ]; then
        MATCH_FOUND=true
        TARGET_CONF_NAME="$model"
        break
    fi
done

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

OLDMOD=/data/adb/modules/Easy_Key
if [ -d "$OLDMOD" ]; then
    ui_print "- 检测到 EasyKey 模块正在更新"
    for name in config.ini repo.json; do
        if [ -f "$OLDMOD/$name" ]; then
            cp -f "$OLDMOD/$name" "$MODPATH/$name" || abort "- 无法保留 $name，安装已取消"
            ui_print "- 已保留 $name"
        fi
    done
    if [ -d "$OLDMOD/backup" ]; then
        cp -R "$OLDMOD/backup" "$MODPATH/backup" || abort "- 无法保留已有脚本备份，安装已取消"
    fi
    if [ -d "$OLDMOD/ind" ]; then
        mkdir -p "$MODPATH/backup" "$MODPATH/ind" || abort "- 无法创建脚本目录，安装已取消"
        backup=$(mktemp -d "$MODPATH/backup/upgrade.XXXXXX") || abort "- 无法创建脚本备份，安装已取消"
        cp -R "$OLDMOD/ind" "$backup/ind" || abort "- 无法备份旧脚本，安装已取消"
        cp -Rn "$OLDMOD/ind/." "$MODPATH/ind/" || abort "- 无法迁移自定义脚本，安装已取消"
        ui_print "- 已保留自定义脚本，内置脚本使用新版"
        ui_print "- 旧脚本备份：$backup/ind"
    fi
fi

}

check
