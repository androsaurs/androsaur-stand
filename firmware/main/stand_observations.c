#include "stand_observations.h"
#include "observation_http.h"
#include "stand_security.h"
#include "stand_tracking.h"
#include "stand_battery.h"
#include "esp_tls.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/tcp.h"
#include <stdatomic.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define OBS_PORT 8444
#define OBS_CLIENTS 2
#define HANDSHAKE_MS 1500
#define REQUEST_MS 200
#define IDLE_MS 15000
typedef struct { atomic_bool busy; int fd; } slot;
static slot slots[OBS_CLIENTS];
static atomic_uint accepted, handshake_errors, request_timeouts, packets, rejected;
static atomic_uint max_handshake_ms, max_request_ms;
static bool started;
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static void maximum(atomic_uint *value,unsigned candidate) {
    unsigned previous=atomic_load(value);
    while (candidate>previous && !atomic_compare_exchange_weak(value,&previous,candidate)) {}
}
static bool wait_socket(int fd,bool writing,int milliseconds) {
    fd_set set; FD_ZERO(&set); FD_SET(fd,&set);
    struct timeval timeout={.tv_sec=milliseconds/1000,.tv_usec=(milliseconds%1000)*1000};
    return select(fd+1,writing?NULL:&set,writing?&set:NULL,NULL,&timeout)>0;
}
static bool would_block(int result) { return result==ESP_TLS_ERR_SSL_WANT_READ || result==ESP_TLS_ERR_SSL_WANT_WRITE; }
static bool send_all(esp_tls_t *tls,int fd,const char *data,size_t size) {
    int64_t deadline=now_ms()+REQUEST_MS;
    while (size && now_ms()<deadline) {
        int n=esp_tls_conn_write(tls,data,size);
        if (n>0) { data+=n; size-=(size_t)n; }
        else if (would_block(n)) wait_socket(fd,n==ESP_TLS_ERR_SSL_WANT_WRITE,10);
        else return false;
    }
    return size==0;
}
static bool reply(esp_tls_t *tls,int fd,int code,bool close,const char *body) {
    // A single TLS record avoids delaying a tiny response body behind its headers.
    char response[512];
    int n=snprintf(response,sizeof(response),"HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: %s\r\n\r\n%s",
        code,code==200?"OK":code==401?"Unauthorized":"Bad Request",(unsigned)strlen(body),close?"close":"keep-alive",body);
    return n>0 && n<(int)sizeof(response) && send_all(tls,fd,response,(size_t)n);
}
static void connection(void *arg) {
    slot *client=arg; int fd=client->fd;
    int no_delay=1;
    setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&no_delay,sizeof(no_delay));
    int flags=fcntl(fd,F_GETFL,0);
    if (flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0) {
        close(fd); atomic_store(&client->busy,false); vTaskDelete(NULL); return;
    }
    esp_tls_t *tls=esp_tls_init();
    if (!tls) { close(fd); atomic_store(&client->busy,false); vTaskDelete(NULL); return; }
    esp_tls_cfg_server_t cfg={.servercert_buf=(const unsigned char *)stand_cert,
        .servercert_bytes=strlen(stand_cert)+1,.serverkey_buf=(const unsigned char *)stand_private_key,
        .serverkey_bytes=strlen(stand_private_key)+1};
    int64_t began=now_ms();
    bool ready=false;
    if (esp_tls_server_session_init(&cfg,fd,tls)==ESP_OK) {
        while (now_ms()-began<HANDSHAKE_MS) {
            int r=esp_tls_server_session_continue_async(tls);
            if (r==0) { ready=true; break; }
            if (!would_block(r)) break;
            wait_socket(fd,r==ESP_TLS_ERR_SSL_WANT_WRITE,10);
        }
    }
    maximum(&max_handshake_ms,(unsigned)(now_ms()-began));
    if (!ready) { atomic_fetch_add(&handshake_errors,1); goto finished; }
    // The established observation stream outranks management HTTP/TLS work.
    vTaskPrioritySet(NULL,6);
    char buffer[OBS_HEADER_MAX+OBS_BODY_MAX+1]; size_t used=0, header_size=0;
    observation_request request;
    int64_t first_byte=0, idle_since=now_ms();
    while (now_ms()-idle_since<IDLE_MS) {
        if (first_byte && now_ms()-first_byte>REQUEST_MS) { atomic_fetch_add(&request_timeouts,1); break; }
        if (!header_size) {
            buffer[used]=0; char *end=strstr(buffer,"\r\n\r\n");
            if (end) {
                header_size=(size_t)(end-buffer)+4;
                if (!observation_http_parse(buffer,header_size,&request)) { reply(tls,fd,400,true,"{\"error\":\"invalid request\"}"); break; }
                if (!stand_authorized(request.token,true)) { atomic_fetch_add(&rejected,1); reply(tls,fd,401,true,"{\"error\":\"unauthorized\"}"); break; }
            } else if (used>=OBS_HEADER_MAX) break;
        }
        if (header_size && used>=header_size+request.content_length) {
            int elapsed=(int)(now_ms()-first_byte);
            cJSON *packet=request.content_length?cJSON_ParseWithLength(buffer+header_size,request.content_length):NULL;
            bool ok=request.route==OBS_STATUS || (request.route==OBS_TRACKING ?
                stand_tracking_observe_delayed(packet,elapsed) : stand_battery_observe(packet));
            cJSON_Delete(packet);
            char owner[65], response[192]; stand_owner_id(owner);
            snprintf(response,sizeof(response),"{\"ok\":%s,\"id\":\"%s\",\"phoneId\":\"%s\"}",ok?"true":"false",stand_id,owner);
            maximum(&max_request_ms,(unsigned)(now_ms()-first_byte));
            if (ok && request.route!=OBS_STATUS) atomic_fetch_add(&packets,1);
            if (!reply(tls,fd,ok?200:400,request.close,response) || request.close) break;
            size_t consumed=header_size+request.content_length;
            memmove(buffer,buffer+consumed,used-consumed); used-=consumed;
            header_size=0; idle_since=now_ms(); first_byte=used?idle_since:0;
            continue;
        }
        size_t capacity=sizeof(buffer)-1-used;
        if (!capacity) break;
        int n=esp_tls_conn_read(tls,buffer+used,capacity);
        if (n>0) {
            if (memchr(buffer+used,0,(size_t)n)) break;
            if (!first_byte) first_byte=now_ms();
            used+=(size_t)n;
        } else if (would_block(n)) wait_socket(fd,n==ESP_TLS_ERR_SSL_WANT_WRITE,10);
        else break;
    }
finished:
    esp_tls_server_session_delete(tls); close(fd);
    atomic_store(&client->busy,false); vTaskDelete(NULL);
}
static void listener(void *arg) {
    int fd=(int)(intptr_t)arg;
    for (;;) {
        int incoming=accept(fd,NULL,NULL);
        if (incoming<0) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        slot *available=NULL;
        for (unsigned i=0;i<OBS_CLIENTS;i++) {
            bool expected=false;
            if (atomic_compare_exchange_strong(&slots[i].busy,&expected,true)) { available=&slots[i]; break; }
        }
        if (!available) { atomic_fetch_add(&rejected,1); close(incoming); continue; }
        available->fd=incoming; atomic_fetch_add(&accepted,1);
        if (xTaskCreate(connection,"stand_obs_peer",8192,available,4,NULL)!=pdPASS) {
            close(incoming); atomic_store(&available->busy,false);
        }
    }
}
void stand_observations_start(void) {
    if (started) return;
    int fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); ESP_ERROR_CHECK(fd>=0?ESP_OK:ESP_FAIL);
    int reuse=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(OBS_PORT),.sin_addr.s_addr=htonl(INADDR_ANY)};
    ESP_ERROR_CHECK(bind(fd,(struct sockaddr *)&address,sizeof(address))==0?ESP_OK:ESP_FAIL);
    ESP_ERROR_CHECK(listen(fd,2)==0?ESP_OK:ESP_FAIL);
    ESP_ERROR_CHECK(xTaskCreate(listener,"stand_obs_accept",3072,(void *)(intptr_t)fd,4,NULL)==pdPASS?ESP_OK:ESP_FAIL);
    started=true;
}
cJSON *stand_observations_status(void) {
    cJSON *p=cJSON_CreateObject();
    cJSON_AddNumberToObject(p,"port",OBS_PORT);
    cJSON_AddNumberToObject(p,"accepted",atomic_load(&accepted));
    cJSON_AddNumberToObject(p,"handshakeErrors",atomic_load(&handshake_errors));
    cJSON_AddNumberToObject(p,"requestTimeouts",atomic_load(&request_timeouts));
    cJSON_AddNumberToObject(p,"packets",atomic_load(&packets));
    cJSON_AddNumberToObject(p,"rejected",atomic_load(&rejected));
    cJSON_AddNumberToObject(p,"maxHandshakeMs",atomic_load(&max_handshake_ms));
    cJSON_AddNumberToObject(p,"maxRequestMs",atomic_load(&max_request_ms));
    return p;
}
