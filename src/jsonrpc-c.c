/*
 * jsonrpc-c.c
 *
 *  Created on: Oct 11, 2012
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

#include "jsonrpc-c.h"
#include "jsonrpc-c-priv.h"

struct ev_loop *loop;

#ifndef offsetof
# define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
# define container_of(ptr, type, member) ( {                            \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);            \
        (type *)( (char *)__mptr - offsetof(type, member) ); } )
#endif

static int __jrpc_ServerStart(jrpc_Server_t * server);
static void jrpc_ProcedureDestroy(jrpc_Procedure_t * procedure);

/**
 * @return A pointer to sockaddr_in or sockaddr_in6
 */
static void * get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

static int send_response(jrpc_Connection_t * conn, char * response) {
	int fd = conn->fd;
	if (conn->debug_level > 1)
		printf("JSON Response:\n%s\n", response);
	write(fd, response, strlen(response));
	write(fd, "\n", 1);
	return 0;
}

static int send_error(jrpc_Connection_t * conn, int code, char * message, cJSON * id) {
	int return_value = 0;
	cJSON *result_root = cJSON_CreateObject();
	cJSON *error_root = cJSON_CreateObject();
	cJSON_AddNumberToObject(error_root, "code", code);
	cJSON_AddStringToObject(error_root, "message", message);
	cJSON_AddItemToObject(result_root, "error", error_root);
	cJSON_AddItemToObject(result_root, "id", id);
	char * str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	free(message);
	return return_value;
}

static int send_result(jrpc_Connection_t * conn, cJSON * result, cJSON * id) {
	int return_value = 0;
	cJSON *result_root = cJSON_CreateObject();

	if (result)
		cJSON_AddItemToObject(result_root, "result", result);

	if (id != NULL)
	    cJSON_AddItemToObject(result_root, "id", id);

	char * str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);

	cJSON_Delete(result_root);
	return return_value;
}

static int send_notification(jrpc_Connection_t * conn, const char * method, cJSON * params) {
    int return_value = 0;
    cJSON *request_root = cJSON_CreateObject();

    cJSON_AddStringToObject(request_root, "jsonrpc", "2.0");

    cJSON_AddStringToObject(request_root, "method", method);

    if(params)
        cJSON_AddItemToObject(request_root, "params", params);

    char * str_result = cJSON_Print(request_root);
    return_value = send_response(conn, str_result);
    free(str_result);

    cJSON_Delete(request_root);
    return return_value;
}

static int invoke_procedure(jrpc_Server_t * server, jrpc_Connection_t * conn, char * name, cJSON * params, cJSON * id) {
	cJSON *returned = NULL;
	int procedure_found = 0;
	jrpc_ProcedureContext_t ctx = {0};

	/* Look for procedure */
	int i = server->procedure_count;
	while (i--) {
		if (!strcmp(server->procedures[i].name, name)) {
			procedure_found = 1;
			ctx.data = server->procedures[i].data;
			returned = server->procedures[i].function(&ctx, params, id);
			break;
		}
	}

	if (!procedure_found)
		return send_error(conn, JRPC_METHOD_NOT_FOUND, strdup("Method not found."), id);
	else {
		if (ctx.error_code)
			return send_error(conn, ctx.error_code, ctx.error_message, id);
		else
			return send_result(conn, returned, id);
	}
}

static int eval_request(jrpc_Server_t *server, jrpc_Connection_t * conn, cJSON *root) {
	cJSON *method, *params, *id;

	method = cJSON_GetObjectItem(root, "method");
	if (method != NULL && method->type == cJSON_String) {

		params = cJSON_GetObjectItem(root, "params");
		if ( (params == NULL)
		        || (params->type == cJSON_Array)
		        || (params->type == cJSON_Object) ) {

			id = cJSON_GetObjectItem(root, "id");
			if ( (id == NULL)
			        || (id->type == cJSON_String)
			        || (id->type == cJSON_Number) ) {
			    // We have to copy ID because using it on the reply and deleting the response Object will also delete ID
				cJSON * id_copy = NULL;
				if (id != NULL) {
					if(id->type == cJSON_String)
					    id_copy = cJSON_CreateString(id->valuestring);
					else
					    id_copy = cJSON_CreateNumber(id->valueint);
				}

				if (server->debug_level)
					printf("Method Invoked: %s\n", method->valuestring);

				return invoke_procedure(server, conn, method->valuestring, params, id_copy);
			}
		}
	}
	send_error(conn, JRPC_INVALID_REQUEST, strdup("The JSON sent is not a valid Request object."), NULL);
	return -1;
}

static void close_connection(struct ev_loop *loop, ev_io *w) {
	ev_io_stop(loop, w);
	close(((jrpc_Connection_t *) w)->fd);
	free(((jrpc_Connection_t *) w)->buffer);
	free(((jrpc_Connection_t *) w));
}

static void connection_cb(struct ev_loop *loop, ev_io *w, int revents) {
	jrpc_Connection_t * conn;
	jrpc_Server_t * server = (jrpc_Server_t *) w->data;
	size_t bytes_read = 0;

    //get our 'subclassed' event watcher
	conn = container_of(w, jrpc_Connection_t, io);

	int fd = conn->fd;
	if (conn->pos == (conn->buffer_size - 1)) {
		char * new_buffer = realloc(conn->buffer, conn->buffer_size *= 2);
		if (new_buffer == NULL) {
			perror("Memory error");
			return close_connection(loop, w);
		}
		conn->buffer = new_buffer;
		memset(conn->buffer + conn->pos, 0, conn->buffer_size - conn->pos);
	}
	// can not fill the entire buffer, string must be NULL terminated
	int max_read_size = conn->buffer_size - conn->pos - 1;
	if ((bytes_read = read(fd, conn->buffer + conn->pos, max_read_size))
			== -1) {
		perror("read");
		return close_connection(loop, w);
	}
	if (!bytes_read) {
		// client closed the sending half of the connection
		if (server->debug_level)
			printf("Client closed connection.\n");

		return close_connection(loop, w);
	} else {
		cJSON *root;
		char *end_ptr;
		conn->pos += bytes_read;

		if ((root = cJSON_Parse_Stream(conn->buffer, &end_ptr)) != NULL) {
			if (server->debug_level > 1) {
				char * str_result = cJSON_Print(root);
				printf("Valid JSON Received:\n%s\n", str_result);
				free(str_result);
			}

			if (root->type == cJSON_Object) {
				eval_request(server, conn, root);
			}
			//shift processed request, discarding it
			memmove(conn->buffer, end_ptr, strlen(end_ptr) + 2);

			conn->pos = strlen(end_ptr);
			memset(conn->buffer + conn->pos, 0, conn->buffer_size - conn->pos - 1);

			cJSON_Delete(root);
		} else {
			// did we parse the all buffer? If so, just wait for more.
			// else there was an error before the buffer's end
			if (cJSON_GetErrorPtr() != (conn->buffer + conn->pos)) {
				if (server->debug_level) {
					printf("INVALID JSON Received:\n---\n%s\n---\n",
							conn->buffer);
				}
				send_error(conn, JRPC_PARSE_ERROR,
						strdup(
								"Parse error. Invalid JSON was received by the server."),
						NULL);
				return close_connection(loop, w);
			}
		}
	}

}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents) {
	char s[INET6_ADDRSTRLEN];
	jrpc_Connection_t * connection_watcher;
	jrpc_Server_t * server = (jrpc_Server_t *)w->data;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size = sizeof(their_addr);

    connection_watcher = malloc(sizeof(jrpc_Connection_t));
    if(connection_watcher == NULL) {
        perror("malloc");
        return;
    }

	connection_watcher->fd = accept(w->fd, (struct sockaddr *) &their_addr, &sin_size);

	if (connection_watcher->fd == -1) {
		perror("accept");
		free(connection_watcher);
	} else {
		if (server->debug_level) {
			inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
			printf("server: got connection from %s\n", s);
		}

		ev_io_init( &(connection_watcher->io), connection_cb, connection_watcher->fd, EV_READ);

		// Copy pointer to jrpc_Server_t
		connection_watcher->io.data = w->data;

		// Allocate buffer
		connection_watcher->buffer_size = JRPC_CONNECTION_BUFFER_SZ;
		connection_watcher->buffer = calloc(1, JRPC_CONNECTION_BUFFER_SZ);
		connection_watcher->pos = 0;

		// Copy debug_level, jrpc_Connection_t has no pointer to jrpc_Server_t
		connection_watcher->debug_level = server->debug_level;

		ev_io_start(loop, &connection_watcher->io);
	}
}

int jrpc_ServerInit(jrpc_Server_t ** server, int port_number) {
    loop = EV_DEFAULT;
    return jrpc_ServerInitWithEvLoop(server, port_number, loop);
}

int jrpc_ServerInitWithEvLoop(jrpc_Server_t ** server, int port_number, struct ev_loop *loop) {
    char * debug_level_env = getenv("JRPC_DEBUG");
    jrpc_Server_t * serverPtr = NULL;

    serverPtr = (jrpc_Server_t *)calloc(1, sizeof(jrpc_Server_t));

	serverPtr->loop = loop;
	serverPtr->port_number = port_number;

	if (debug_level_env == NULL)
		serverPtr->debug_level = 0;
	else {
		serverPtr->debug_level = strtol(debug_level_env, NULL, 10);
		printf("JSONRPC-C Debug level %d\n", serverPtr->debug_level);
	}

	/* Return reference */
	*server = serverPtr;

	return __jrpc_ServerStart(serverPtr);
}

static int __jrpc_ServerStart(jrpc_Server_t * server) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int yes = 1;
	int rv;
	char PORT[6];
	sprintf(PORT, "%d", server->port_number);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
				== -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, 5) == -1) {
		perror("listen");
		exit(1);
	}

	if (server->debug_level)
		printf("server: waiting for connections...\n");

	ev_io_init(&server->listen_watcher, accept_cb, sockfd, EV_READ);
	server->listen_watcher.data = server;

	ev_io_start(server->loop, &server->listen_watcher);
	return 0;
}

// Make the code work with both the old (ev_loop/ev_unloop)
// and new (ev_run/ev_break) versions of libev.

#ifdef EVUNLOOP_ALL
  #define EV_RUN ev_loop
  #define EV_BREAK ev_unloop
  #define EVBREAK_ALL EVUNLOOP_ALL
#else
  #define EV_RUN ev_run
  #define EV_BREAK ev_break
#endif

void jrpc_ServerRun(jrpc_Server_t * server) {
	EV_RUN(server->loop, 0);
}

int jrpc_ServerStop(jrpc_Server_t * server) {
	EV_BREAK(server->loop, EVBREAK_ALL);
	return 0;
}

void jrpc_ServerDestroy(jrpc_Server_t ** server) {
	/* Don't destroy server */
	int i;

	/* Destroy all procedures */
	for (i = 0; i < (*server)->procedure_count; i++){
		jrpc_ProcedureDestroy( &((*server)->procedures[i]) );
	}
	free((*server)->procedures);

	*server = NULL;
}

static void jrpc_ProcedureDestroy(jrpc_Procedure_t *procedure) {
	if (procedure->name){
		free(procedure->name);
		procedure->name = NULL;
	}
	if (procedure->data){
		free(procedure->data);
		procedure->data = NULL;
	}
}

int jrpc_ProcedureRegister(jrpc_Server_t * server, jrpc_ProcedureHandler_t function_pointer, const char * name, void * data) {
    jrpc_Procedure_t * procedure;
	int i = server->procedure_count++;

	if (!server->procedures)
		server->procedures = malloc( sizeof(jrpc_Procedure_t) );
	else {
		jrpc_Procedure_t * ptr = realloc(server->procedures,
				sizeof(jrpc_Procedure_t) * server->procedure_count);
		if (!ptr)
			return -1;
		server->procedures = ptr;
	}

	procedure = &(server->procedures[i]);

	if ((procedure->name = strdup(name)) == NULL) {
	    procedure->function = NULL;
        return -1;
	}

	procedure->function = function_pointer;
	procedure->data = data;

	return 0;
}

int jrpc_ProcedureUnregister(jrpc_Server_t * server, const char * name) {

	int i;
	int found = 0;

	if (server->procedures) {
	    /* Search the procedure to unregister */
		for (i = 0; i < server->procedure_count; i++) {
			if (found)
				server->procedures[i-1] = server->procedures[i];
			else if( (server->procedures[i].function != NULL)
			        && (strcmp(name, server->procedures[i].name) == 0) ) {
				found = 1;
				jrpc_ProcedureDestroy( &(server->procedures[i]) );
			}
		}


		if (found) {
			server->procedure_count--;
			if (server->procedure_count) {
				jrpc_Procedure_t * ptr = realloc(server->procedures,
					sizeof(jrpc_Procedure_t) * server->procedure_count);
				if (!ptr) {
					perror("realloc");
					return -1;
				}
				server->procedures = ptr;
			} else {
				server->procedures = NULL;
			}
		}
	}

	if (!found) {
		fprintf(stderr, "server : procedure '%s' not found\n", name);
		return -1;
	}

	return 0;
}

void jrpc_ClientStartOnConnection(jrpc_ClientRef_t * client, jrpc_ConnectionRef_t connection) {
    jrpc_Client_t * clientPtr = NULL;

    clientPtr = (jrpc_Client_t *)malloc( sizeof(jrpc_Client_t) );
    clientPtr->connection = connection;

    *client = clientPtr;
}

int jrpc_ClientSendNotification(jrpc_ClientRef_t client, const char * method, cJSON * params) {
    return send_notification(client->connection, method, params);
}

void jrpc_ClientDestroy(jrpc_ClientRef_t * client) {
    free(*client);
    *client = NULL;
}
