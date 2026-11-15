#include "observation_http.h"
#include <string.h>
#include <strings.h>

bool observation_http_parse(char *header, size_t length, observation_request *out) {
    if (!header || !out || length<4 || length>OBS_HEADER_MAX ||
        memchr(header,0,length) || memcmp(header+length-4,"\r\n\r\n",4)) return false;
    memset(out,0,sizeof(*out));
    char *end=strstr(header,"\r\n");
    if (!end) return false;
    *end=0;
    if (!strcmp(header,"GET /v1/status HTTP/1.1")) out->route=OBS_STATUS;
    else if (!strcmp(header,"POST /v1/tracking HTTP/1.1")) out->route=OBS_TRACKING;
    else if (!strcmp(header,"POST /v1/battery HTTP/1.1")) out->route=OBS_BATTERY;
    else return false;
    bool auth=false, sized=false, host=false;
    for (char *line=end+2; *line!='\r'; line=end+2) {
        end=strstr(line,"\r\n"); if (!end) return false; *end=0;
        char *colon=strchr(line,':'); if (!colon || colon==line) return false;
        *colon=0;
        // Reject whitespace/invalid characters in field names and folded headers.
        if (strspn(line,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")!=strlen(line)) return false;
        char *value=colon+1; while (*value==' ' || *value=='\t') value++;
        char *tail=value+strlen(value); while (tail>value && (tail[-1]==' ' || tail[-1]=='\t')) *--tail=0;
        if (!strcasecmp(line,"Authorization")) {
            if (auth || strncmp(value,"Bearer ",7) || strlen(value)!=71 ||
                strspn(value+7,"0123456789abcdef")!=64) return false;
            memcpy(out->token,value+7,65); auth=true;
        } else if (!strcasecmp(line,"Content-Length")) {
            if (sized || !*value) return false;
            size_t size=0;
            for (const char *p=value; *p; p++) {
                if (*p<'0' || *p>'9') return false;
                size=size*10+(unsigned)(*p-'0'); if (size>OBS_BODY_MAX) return false;
            }
            out->content_length=size; sized=true;
        } else if (!strcasecmp(line,"Host")) {
            if (host || !*value) return false;
            host=true;
        } else if (!strcasecmp(line,"Transfer-Encoding") || !strcasecmp(line,"Expect")) return false;
        else if (!strcasecmp(line,"Connection")) out->close=!strcasecmp(value,"close");
    }
    return auth && host && (out->route==OBS_STATUS ? out->content_length==0 : sized && out->content_length>=2);
}
