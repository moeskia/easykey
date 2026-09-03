#!/system/bin/sh

state=$(dumpsys activity service com.android.systemui/.SystemUIService 2>/dev/null | grep -m 1 'mFlashlightEnabled=')
case "$state" in
    *=true*) close=true ;;
    *) close=false ;;
esac
am broadcast --user 0 -a com.android.systemui.ACTION_SWITCH_FLASHLIGHT --ez intent_extra_flashlight "$close" >/dev/null 2>&1
