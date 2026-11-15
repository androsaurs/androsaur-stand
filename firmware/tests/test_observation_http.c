#include "observation_http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *key="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static bool parse(const char *method,const char *headers,observation_request *out) {
    char raw[OBS_HEADER_MAX+1];
    int n=snprintf(raw,sizeof(raw),"%s\r\nHost: stand:8444\r\nAuthorization: Bearer %s\r\n%s\r\n",method,key,headers);
    assert(n>0 && (size_t)n<sizeof(raw)); return observation_http_parse(raw,(size_t)n,out);
}
int main(void) {
    observation_request r;
    assert(parse("POST /v1/tracking HTTP/1.1","Content-Length: 128\r\n",&r));
    assert(r.route==OBS_TRACKING && r.content_length==128 && !strcmp(r.token,key) && !r.close);
    assert(parse("POST /v1/battery HTTP/1.1","content-length: 512\r\nConnection: close\r\n",&r));
    assert(r.route==OBS_BATTERY && r.close && r.content_length==512);
    assert(parse("GET /v1/status HTTP/1.1","",&r) && r.route==OBS_STATUS);
    const char *invalid[]={"Content-Length: 513\r\n","Content-Length: -1\r\n",
        "Content-Length: 2x\r\n","Content-Length: 99999999999999999999999999999\r\n",
        "Content-Length: 2\r\nContent-Length: 2\r\n","Content-Length: 2\r\nTransfer-Encoding: chunked\r\n",
        "Content-Length: 2\r\nExpect: 100-continue\r\n","Content-Length: 2\r\nHost: other\r\n",
        "Content-Length : 2\r\n","Content-Length: 2\r\n folded\r\n","Content-Length: 0\r\n",""};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) assert(!parse("POST /v1/tracking HTTP/1.1",invalid[i],&r));
    assert(!parse("POST /v1/servos HTTP/1.1","Content-Length: 2\r\n",&r));
    assert(!parse("GET /v1/status HTTP/1.1","Content-Length: 2\r\n",&r));
    char no_auth[]="GET /v1/status HTTP/1.1\r\nHost: stand\r\n\r\n";
    assert(!observation_http_parse(no_auth,strlen(no_auth),&r));
    char embedded[]="GET /v1/status HTTP/1.1\r\nX: a\0b\r\n\r\n";
    assert(!observation_http_parse(embedded,sizeof(embedded)-1,&r));
    // Deterministic malformed-input fuzzing, run under ASan/UBSan on the host.
    unsigned random=12345;
    for (unsigned run=0;run<10000;run++) {
        char raw[OBS_HEADER_MAX+1];
        size_t length=run%(OBS_HEADER_MAX+1);
        for (size_t i=0;i<length;i++) { random=random*1664525u+1013904223u; raw[i]=(char)(random>>24); }
        raw[length]=0;
        if (length>=4 && run%2) memcpy(raw+length-4,"\r\n\r\n",4);
        (void)observation_http_parse(raw,length,&r);
    }
    puts("observation HTTP: routes, owner token, bounded lengths, duplicate/smuggling/invalid-header rejection OK");
}
