#include "charger_manual_core.h"
bool charger_override_set(charger_override *s, bool enabled, int duration_ms, int64_t now_ms) {
    if (duration_ms<1000 || duration_ms>CHARGER_MANUAL_MAX_MS || now_ms<0) return false;
    s->active=true; s->enabled=enabled; s->until_ms=now_ms+duration_ms;
    return true;
}
bool charger_override_apply(charger_override *s, bool automatic_enabled, bool policy_allow, int64_t now_ms) {
    if (s->active && now_ms>=s->until_ms) s->active=false;
    return s->active ? s->enabled : automatic_enabled && policy_allow;
}
