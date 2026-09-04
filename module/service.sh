#!/system/bin/sh

resetprop -w sys.boot_completed 0

MODDIR=${0%/*}

chmod 755 "$MODDIR/EasyKey"
pkill -x EasyKey 2>/dev/null
nohup "$MODDIR/EasyKey" >/dev/null 2>&1 &
