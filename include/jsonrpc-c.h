/*
 * jsonrpc-c.h
 *
 *  Created on: Oct 11, 2012
 *      Author: hmng
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

typedef struct jrpc_Context {
	void *data;
	int error_code;
	char * error_message;
} jrpc_Context_t;

typedef cJSON* (*jrpc_Function)(jrpc_Context_t *context, cJSON *params, cJSON* id);

typedef struct jrpc_Procedure {
	char * name;
	jrpc_Function function;
	void *data;
} jrpc_Procedure_t;

typedef struct jrpc_Server {
	int port_number;
	struct ev_loop *loop;
	ev_io listen_watcher;
	int procedure_count;
	jrpc_Procedure_t *procedures;
	int debug_level;
} jrpc_Server_t;

typedef struct jrpc_Connection {
	struct ev_io io;
	int fd;
	int pos;
	unsigned int buffer_size;
	char * buffer;
	int debug_level;
} jrpc_Connection_t;

/**
 * @defgroup ServerMgmt Server Management
 * @{
 */

int jrpc_ServerInit(jrpc_Server_t *server, int port_number);

int jrpc_ServerInitWithEvLoop(jrpc_Server_t *server, int port_number, struct ev_loop *loop);

void jrpc_ServerRun(jrpc_Server_t *server);

int jrpc_ServerStop(jrpc_Server_t *server);

void jrpc_ServerDestroy(jrpc_Server_t *server);

/* @} */

/**
 * @defgroup ProcedureMgmt Procedure Management
 * @{
 */

int jrpc_ProcedureRegister(jrpc_Server_t *server, jrpc_Function function_pointer, char *name, void *data);

int jrpc_ProcedureUnregister(jrpc_Server_t *server, char *name);

/* @} */

#endif
