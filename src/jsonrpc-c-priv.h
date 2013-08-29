/*
 * @file jsonrpc-c-priv.h
 *
 * JSON RPC Private Declarations
 */

#ifndef JSONRPC_C_PRIV_H_
#define JSONRPC_C_PRIV_H_

#define JRPC_CONNECTION_BUFFER_SZ   1500

typedef struct jrpc_Procedure {
    char * name;
    jrpc_ProcedureHandler_t function;
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


#endif /* JSONRPC_C_PRIV_H_ */
