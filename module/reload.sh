#!/data/adb/ksu/bin/busybox sh

MODDIR=${0%/*}
exec 9>"$MODDIR/.reload.lock" || exit 1
/data/adb/ksu/bin/busybox flock -n 9 || { echo "正在重载或无法获取重载锁，请稍后重试" >&2; exit 1; }
chmod 755 "$MODDIR/EasyKey" || exit 1
for group in /acct /dev/cg2_bpf /sys/fs/cgroup /dev/memcg/apps; do
    [ -e "$group/cgroup.procs" ] || continue
    printf '%s\n' "$$" >> "$group/cgroup.procs" || { echo "无法脱离管理器 cgroup：$group" >&2; exit 1; }
done
while IFS=: read -r hierarchy controllers group; do
    case "$group" in
        */uid_*|*/pid_*|/apps/*)
            echo "仍在应用 cgroup 中，取消重载：$group" >&2
            exit 1
            ;;
    esac
done < "/proc/$$/cgroup" || { echo "无法检查重载进程 cgroup" >&2; exit 1; }
pids=$(pidof EasyKey)
[ -z "$pids" ] || kill $pids 2>/dev/null
attempt=0
while pidof EasyKey >/dev/null 2>&1; do
    [ "$attempt" -lt 20 ] || { echo "旧进程未退出，未启动新进程" >&2; exit 1; }
    sleep 0.1
    attempt=$((attempt + 1))
done
nohup setsid "$MODDIR/EasyKey" 9>&- </dev/null >/dev/null 2>&1 &
pid=$!
sleep 0.2
kill -0 "$pid" 2>/dev/null || { echo "EasyKey 启动失败" >&2; exit 1; }
printf 'easykey_reload_ok\n'
