#!/system/bin/sh

resetprop -w sys.boot_completed 0
MODDIR=${0%/*}
/data/adb/ksu/bin/busybox sh "$MODDIR/reload.sh" >/dev/null 2>&1
