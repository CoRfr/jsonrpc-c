/*
 * @file jsonrpc-c-priv.h
 *
 * JSON RPC Public Declarations
 */

#ifndef JSONRCPC_H_
#define JSONRPCC_H_

#include "cJSON.h"
#include <ev.h>

/*
 *
 * http://www.jsonrpc.org/specification
 *
 * code	message	meaning
 * -32700	Parse error	Invalid JSON was received by the server.
 * An error occurred on the server while parsing the JSON text.
 * -32600	Invalid Request	The JSON sent is not a valid Request object.
 * -32601	Method not found	The method does not exist / is not available.
 * -32602	Invalid params	Invalid method parameter(s).
 * -32603	Internal error	Internal JSON-RPC error.
 * -32000 to -32099	Server error	Reserved for implementation-defined server-errors.
 */

#define JRPC_PARSE_ERROR -32700
#define JRPC_INVALID_REQUEST -32600
#define JRPC_METHOD_NOT_FOUND -32601
#define JRPC_INVALID_PARAMS -32603
#define JRPC_INTERNAL_ERROR -32693


typedef struct jrpc_Server * jrpc_ServerRef_t;
typedef struct jrpc_Connection * jrpc_ConnectionRef_t;

typedef struct jrpc_ProcedureContext {
    jrpc_ConnectionRef_t connection;
    void * data;
    int error_code;
    char * error_message;
} jrpc_ProcedureContext_t;

typedef cJSON* (*jrpc_ProcedureHandler_t)(jrpc_ProcedureContext_t * context, cJSON * params, cJSON * id);

/**
 * @defgroup ServerMgmt RPC Server Management
 * @{
 */

int jrpc_ServerInit(jrpc_ServerRef_t * server, int port_number);

int jrpc_ServerInitWithEvLoop(jrpc_ServerRef_t * server, int port_number, struct ev_loop * loop);

void jrpc_ServerRun(jrpc_ServerRef_t server);

int jrpc_ServerStop(jrpc_ServerRef_t server);

void jrpc_ServerDestroy(jrpc_ServerRef_t * server);

/* @} */

/**
 * @defgroup ProcedureMgmt Procedure Management
 * @{
 */

int jrpc_ProcedureRegister(jrpc_ServerRef_t server, jrpc_ProcedureHandler_t function_pointer, const char * name, void * data);

int jrpc_ProcedureUnregister(jrpc_ServerRef_t server, const char * name);

/* @} */

/**
 * @defgroup ClientMgmt RPC Client Management
 * @{
 */

typedef struct jrpc_Client {
    jrpc_ConnectionRef_t connection;
} jrpc_Client_t;

typedef struct jrpc_Client * jrpc_ClientRef_t;

//int jrpc_ClientStart(jrpc_ClientRef_t client, char * host, int port_number);

/**
 * Reuse connection from server and declares it as a client.
 */
void jrpc_ClientStartOnConnection(jrpc_ClientRef_t * client, jrpc_ConnectionRef_t connection);

int jrpc_ClientSendNotification(jrpc_ClientRef_t client, const char * method, cJSON * params);

void jrpc_ClientDestroy(jrpc_ClientRef_t * client);

/* @} */


#endif
