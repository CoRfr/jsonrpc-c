/*
 * server.c
 *
 *  Created on: Oct 9, 2012
 *      Author: hmng
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include "jsonrpc-c.h"

#define PORT 1234  // the port users will be connecting to

jrpc_ServerRef_t myServer;

cJSON * say_hello(jrpc_ProcedureContext_t * ctx, cJSON * params, cJSON * id) {
	return cJSON_CreateString("Hello!");
}

cJSON * exit_server(jrpc_ProcedureContext_t * ctx, cJSON * params, cJSON * id) {
	jrpc_ServerStop(myServer);
	return cJSON_CreateString("Bye!");
}

typedef struct {
    int delay;
    jrpc_ConnectionRef_t connection;
} notify_me_ctx_t;

void * notify_me_thread(void * ctx) {
    notify_me_ctx_t * notify_me_ctx = (notify_me_ctx_t *)ctx;
    jrpc_ClientRef_t client;

    printf("Notify thread");

    sleep(notify_me_ctx->delay);

    printf("Notifying ...");

    jrpc_ClientStartOnConnection(&client, notify_me_ctx->connection);
    jrpc_ClientSendNotification(client, "notify", cJSON_CreateString("Notified !"));
    jrpc_ClientDestroy(&client);

    return NULL;
}

cJSON * notify_me(jrpc_ProcedureContext_t * ctx, cJSON * params, cJSON * id) {
    notify_me_ctx_t * notify_me_ctx;
    pthread_t thread_id;

    notify_me_ctx = (notify_me_ctx_t*)malloc(sizeof(notify_me_ctx_t));
    if(notify_me_ctx == NULL)
        return cJSON_CreateFalse();

    notify_me_ctx->delay = 2;
    notify_me_ctx->connection = ctx->connection;

    if(params != NULL) {
        cJSON * delayObj = cJSON_GetObjectItem(params, "delay");
        if(delayObj != NULL) {
            if(delayObj->type == cJSON_Number) {
                notify_me_ctx->delay = delayObj->valueint;
            }
        }
    }

    if( 0 != pthread_create(&thread_id, NULL, notify_me_thread, notify_me_ctx) )
        return cJSON_CreateFalse();

    return cJSON_CreateTrue();
}

int main(void) {
	jrpc_ServerInit(&myServer, PORT);
	jrpc_ProcedureRegister(myServer, say_hello, "sayHello", NULL );
    jrpc_ProcedureRegister(myServer, notify_me, "notifyMe", NULL );
	jrpc_ProcedureRegister(myServer, exit_server, "exit", NULL );
	jrpc_ServerRun(myServer);
	jrpc_ServerDestroy(&myServer);
	return 0;
}
