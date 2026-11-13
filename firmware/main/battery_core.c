#include "battery_core.h"
#include <string.h>
void battery_init(battery_controller *s, int low, int high) {
    memset(s,0,sizeof(*s)); s->low=low; s->high=high; s->allow=true;
}
void battery_tick(battery_controller *s, int64_t now) {
    if (!s->received || now-s->received_ms+s->age_ms>600000) s->allow=true;
}
bool battery_observe(battery_controller *s, const char *session, uint64_t seq, int percent, bool charging, int age, int64_t now) {
    if (!session || strlen(session)!=36 || percent<0 || percent>100 || age<0 || age>60000 ||
        s->low<0 || s->low>=s->high || s->high>100) return false;
    battery_tick(s,now);
    if (strcmp(session,s->session)) {
        for (unsigned i=0;i<8;i++) if (!strcmp(session,s->retired[i])) return false;
        if (s->session[0]) strcpy(s->retired[s->retired_next++%8],s->session);
        strcpy(s->session,session); s->received=false;
    }
    if (s->received && seq<=s->seq) return false;
    s->received=true; s->seq=seq; s->percent=percent; s->charging=charging;
    s->received_ms=now; s->age_ms=age;
    if (percent>=s->high) s->allow=false;
    else if (percent<=s->low) s->allow=true;
    return true;
}
