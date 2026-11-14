#include "charger_manual_core.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    charger_override s={0};
    assert(!charger_override_apply(&s,false,true,0));
    assert(!charger_override_set(&s,true,999,0));
    assert(!charger_override_set(&s,true,120001,0));
    assert(charger_override_set(&s,true,60000,100));
    assert(charger_override_apply(&s,false,false,60099));
    assert(!charger_override_apply(&s,false,true,60100));
    assert(!s.active);
    assert(charger_override_set(&s,false,1000,70000));
    assert(!charger_override_apply(&s,true,true,70999));
    assert(charger_override_apply(&s,true,true,71000));
    assert(charger_override_set(&s,true,1000,80000));
    assert(!charger_override_apply(&s,true,false,81000));
    puts("charger override: defaults off, duration bounds, expiry and auto fallback OK");
}
