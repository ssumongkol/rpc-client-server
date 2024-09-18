#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>
#include "rpc.h"
#include "function.h"

#define MIN_PORT_VALUE 0
#define MAX_PORT_VALUE 99999
#define MIN_FNAME_LEN 1
#define MAX_FNAME_LEN 1000
#define MIN_FNAME_ASCII 32
#define MAX_FNAME_ASCII 126
#define RPC_FIND_FLAG 1
#define RPC_CALL_FLAG 2
#define RPC_CLOSE_CLIENT_FLAG 0
#define HEADER_BUFFER_SIZE (2 * sizeof(uint16_t))
#define UINT16_SIZE sizeof(uint16_t)
#define UINT32_SIZE sizeof(uint32_t)
#define UINT64_SIZE sizeof(uint64_t)
#define RPC_DATA_NULL_DATA2_SIZE UINT64_SIZE

void loadRPCDataToBuffer(rpc_data *payload, char *buffer_pointer);
void extractRPCDataFromBuffer(rpc_data *payload, char *buffer_pointer, uint32_t payload_len);
uint64_t hton64bit(uint64_t data);
uint64_t n64bittoh(uint64_t data);

struct rpc_server {
    int sockfd;
    fd_set masterfds;
    int maxfd;
    functionList_t *functionList;
};

/* Initialises server state */
/* RETURNS: rpc_server* on success, NULL on error */
/* code inspired from COMP30023 Workshop9 */
rpc_server *rpc_init_server(int port) {
	if (port < MIN_PORT_VALUE || port > MAX_PORT_VALUE) {
		return NULL;
	}

	char port_str[5];
    int re, s, sockfd;
	struct addrinfo hints, *res;

	sprintf(port_str, "%d", port);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;       // IPv6
	hints.ai_socktype = SOCK_STREAM; // Connection-mode byte streams (TCP)
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept

	s = getaddrinfo(NULL, port_str, &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return NULL;
	}

	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		perror("socket");
		return NULL;
	}

	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("setsockopt");
		return NULL;
	}

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		return NULL;
	}
	freeaddrinfo(res);

    // initialise rpc_server for storing server information
    rpc_server *server = malloc(sizeof(*server));
    assert(server);
    server->functionList = functionListCreate();
	assert(server->functionList);
    server->sockfd = sockfd;
	
    // initialise file descriptor set
    FD_ZERO(&(server->masterfds));
    FD_SET(server->sockfd, &(server->masterfds));
    server->maxfd = sockfd;

    return server;
}

/* Registers a function (mapping from name to handler) */
/* RETURNS: -1 on failure */
int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
	int fname_len = strlen(name);
    
	if (srv == NULL || name == NULL || handler == NULL || 
	fname_len > MAX_FNAME_LEN || fname_len < MIN_FNAME_LEN) {
        return -1;
    }

    for (int i = 0; i < fname_len; i++) {
        if (name[i] < MIN_FNAME_ASCII || name[i] > MAX_FNAME_ASCII) {
            return -1;
        }
    }

    // initalise function object to store function info. and add into functionList in rpc_server
    function_t *function = functionCreate(fname_len);
	assignNameToFunction(function, name);
    assignRPCHandlerToFunction(function, handler);
    functionRegister(srv->functionList, function);
    
    return getFidFunction(function);
}

/* Start serving requests */
/* packet serialization inspired from beej's guide (https://beej.us/guide/bgnet/html/#htonsman) */
/* and https://robinmoussu.gitlab.io/blog/post/binary_serialisation_of_enum/ */
/* code inspired from COMP30023 Workshop10 */
void rpc_serve_all(rpc_server *srv) {
	if (srv == NULL) {
		return;
	}

	if (listen(srv->sockfd, 10) < 0) {
		perror("listen");
		return;
	}

    while (1) {
        // monitor file descriptors
	    fd_set readfds = srv->masterfds;
		if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0) {
			perror("select");
			return;
		}

		// loop all possible descriptor
		for (int i = 0; i <= srv->maxfd; ++i) {
			// determine if the current file descriptor is active
			if (!FD_ISSET(i, &readfds)) {
				continue;
			}

			// create new socket if there is new incoming connection request to listening interface
			if (i == srv->sockfd) {
				struct sockaddr_in6 cliaddr;
				socklen_t clilen = sizeof(cliaddr);
				int newsockfd = accept(srv->sockfd, (struct sockaddr*)&cliaddr, &clilen);
				if (newsockfd < 0)
					perror("accept");
				else {
					// add the socket to the file descriptor set
					FD_SET(newsockfd, &(srv->masterfds));
					// update the maximum tracker
					if (newsockfd > srv->maxfd)
						srv->maxfd = newsockfd;
					// print out the IP and the socket number
					char ip[INET6_ADDRSTRLEN];
					fprintf(stderr, "new connection from %s on socket %d\n",
						   // convert to human readable string
						   inet_ntop(cliaddr.sin6_family, &cliaddr.sin6_addr, ip,
									 INET6_ADDRSTRLEN),
						   newsockfd);
				}
			}
			// client called rpc_find() / rpc_called()
			else {
				// read header_buffer from client
				char header_buffer[HEADER_BUFFER_SIZE];
				char *ptr = header_buffer;
				int n = read(i, header_buffer, HEADER_BUFFER_SIZE);
				if (n <= 0) {
					if (n < 0)
						perror("read");
					continue;
				}

				// extract function_flag from buffer
				uint16_t flag_network, flag;
				memcpy(&flag_network, ptr, sizeof(flag_network));
				flag = ntohs(flag_network);
				ptr += sizeof(flag_network);

				// rpc_find()
				if (flag == 1) {
					// extract fname_len from header_buffer
					uint16_t fname_len_network, fname_len;
					memcpy(&fname_len_network, ptr, sizeof(fname_len_network));
					fname_len = ntohs(fname_len_network);

					// read fname from client & search for matching function
					char fname_buffer[fname_len];
					n = read(i, fname_buffer, fname_len);
					if (n <= 0) {
						if (n < 0)
							perror("read");
						continue;
					}
					fname_buffer[fname_len] = '\0';
					uint16_t fid = searchFunction(srv->functionList, fname_buffer);

					// sending response (fid) to client
					char res_buffer[UINT16_SIZE];
					uint16_t fid_network = htons(fid);
					memcpy(res_buffer, &fid_network, sizeof(fid_network));
					n = write(i, res_buffer, UINT16_SIZE);
					if (n < 0) {
						perror("write");
						continue;
					}
				} 
				// rpc_call()
				else if (flag == 2) {
					// extract fid from header buffer
					uint16_t fid_network, fid;
					memcpy(&fid_network, ptr, sizeof(fid_network));
					fid = ntohs(fid_network);

					// read rpc_data_len from client
					char rpc_data_len_buffer[UINT32_SIZE];
					ptr = rpc_data_len_buffer;
					n = read(i, rpc_data_len_buffer, UINT32_SIZE);
					if (n <= 0) {
						if (n < 0)
							perror("read");
						continue;
					}
					uint32_t rpc_data_len_network, rpc_data_len;
					memcpy(&rpc_data_len_network, ptr, sizeof(rpc_data_len_network));
					rpc_data_len = ntohl(rpc_data_len_network);
					
					// read rpc_data from client & extract to input_rpc_data
					rpc_data *input_rpc_data = malloc(sizeof(*input_rpc_data));
					char in_rpc_data_buffer[rpc_data_len];
					ptr = in_rpc_data_buffer;
					n = read(i, in_rpc_data_buffer, rpc_data_len);
					if (n <= 0) {
						if (n < 0)
							perror("read");
						continue;
					}
					extractRPCDataFromBuffer(input_rpc_data, ptr, rpc_data_len);

					// process function
					rpc_handler called_function = getHandlerFunctionList(srv->functionList, fid);
					rpc_data *res_rpc_data = called_function(input_rpc_data);
					free(input_rpc_data);

					// determine total_res_size & sending total_res_size to client
					uint32_t total_res_size;
					if (res_rpc_data == NULL || ((res_rpc_data->data2_len > 0) & (res_rpc_data->data2 == NULL)) || 
					((res_rpc_data->data2_len == 0) & (res_rpc_data->data2 != NULL))) {
						total_res_size = 0;
					} else if (res_rpc_data->data2_len == 0) {
						total_res_size = UINT64_SIZE;
					} else {
						total_res_size = UINT64_SIZE + UINT32_SIZE + res_rpc_data->data2_len;
					}
					
					char res_data_size_buffer[UINT32_SIZE];
					ptr = res_data_size_buffer;
					uint32_t total_res_size_network = htonl(total_res_size);
					memcpy(ptr, &total_res_size_network, sizeof(total_res_size_network));
					n = write(i, res_data_size_buffer, UINT32_SIZE);
					if (n < 0) {
						perror("write");
						continue;
					} else if (total_res_size == 0) {
						// if the total_res_size == 0, mean return_rpc_data is invalid
						// Thus, the system continue to the next process
						fprintf(stderr, "invalid return for return_rpc_data, move to the next process");
						continue;
					}

					// send res_data back to client
					char res_data_buffer[total_res_size];
					ptr = res_data_buffer;
					loadRPCDataToBuffer(res_rpc_data, ptr);
					n = write(i, res_data_buffer, total_res_size);
					if (n < 0) {
						perror("write");
						continue;
					}
				} 
				// rpc_close_client()
				else {
					fprintf(stderr, "socket %d closed the connection\n", i);
					close(i);
					FD_CLR(i, &(srv->masterfds));
				}
			}
		}
    }

}

struct rpc_client {
	int sockfd;
};

struct rpc_handle {
	int fid;
};

/* Initialises server state */
/* RETURNS: rpc_server* on success, NULL on error */
/* code inspired from COMP30023 Workshop9 */
rpc_client *rpc_init_client(char *addr, int port) {
	if (port < MIN_PORT_VALUE || port > MAX_PORT_VALUE) {
		return NULL;
	}

	char port_str[5];
	int sockfd, s;
	struct addrinfo hints, *servinfo, *rp;

	sprintf(port_str, "%d", port);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // IPv6
	hints.ai_socktype = SOCK_STREAM; // Connection-mode byte streams (TCP)

	s = getaddrinfo(addr, port_str, &hints, &servinfo);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return NULL;
	}

	for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1)
			continue;
		if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; // success
		close(sockfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return NULL;
	}
	freeaddrinfo(servinfo);

	// initialise rpc_client for storing client information
    rpc_client *client = malloc(sizeof(*client));
    assert(client);
	client->sockfd = sockfd;
	
    return client;
}

/* Finds a remote function by name */
/* RETURNS: rpc_handle* on success, NULL on error */
/* rpc_handle* will be freed with a single call to free(3) */
/* packet serialization inspired from beej's guide (https://beej.us/guide/bgnet/html/#htonsman) */
/* and https://robinmoussu.gitlab.io/blog/post/binary_serialisation_of_enum/ */
rpc_handle *rpc_find(rpc_client *cl, char *name) {
	int fname_len = strlen(name);

	if (cl == NULL || name == NULL || fname_len > MAX_FNAME_LEN || 
	fname_len < MIN_FNAME_LEN) {
        return NULL;
    }

    for (int i = 0; i < fname_len; i++) {
        if (name[i] < MIN_FNAME_ASCII || name[i] > MAX_FNAME_ASCII) {
            return NULL;
        }
    }
	
	// rpc_find() will sent 3 data
	// 1.(uint16_t *) function_flag: to indicate which function is called
	// 2.(uint16_t *) fname_len: to indicate the length of searched function name
	// 3.fname (fname_len byte): actual searched function name

	// serialize header_buffer which contains function_flag & fname_len
	char header_buffer[HEADER_BUFFER_SIZE];
	char *ptr = header_buffer;
	uint16_t function_flag_network = htons(RPC_FIND_FLAG);
	memcpy(ptr, &function_flag_network, sizeof(function_flag_network));
	ptr += sizeof(function_flag_network);

	uint16_t fname_len_network = htons(fname_len);
	memcpy(ptr, &fname_len_network, sizeof(fname_len_network));

	// send header_buffer to server
	int n = write(cl->sockfd, header_buffer, HEADER_BUFFER_SIZE);
	if (n < 0) {
		perror("socket");
		return NULL;
	}

	// send fname to server
	n = write(cl->sockfd, name, fname_len);
	if (n < 0) {
		perror("socket");
		return NULL;
	}
	
	// read respond (fid) from server
	char res_buffer[UINT16_SIZE];
	n = read(cl->sockfd, res_buffer, UINT16_SIZE);
	if (n < 0) {
		perror("read");
		return NULL;
	}
	ptr = res_buffer;
	uint16_t fid_network;
	memcpy(&fid_network, ptr, sizeof(fid_network));
	uint16_t fid = ntohs(fid_network);

	if (fid == 0) {
		return NULL;
	}

	// stored fid in rpc_handle
	rpc_handle *res = malloc(sizeof(*res));
	res->fid = fid;

	return res;
}

/* Calls remote function using handle */
/* RETURNS: rpc_data* on success, NULL on error */
/* packet serialization inspired from beej's guide (https://beej.us/guide/bgnet/html/#htonsman) */
/* and https://robinmoussu.gitlab.io/blog/post/binary_serialisation_of_enum/ */
rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
	if (cl == NULL || h == NULL || payload == NULL || ((payload->data2_len > 0) & (payload->data2 == NULL))
	|| ((payload->data2_len == 0) & (payload->data2 != NULL))) {
		return NULL;
	}

	// rpc_call() will sent 4 data
	// 1.(uint16_t *) function_flag: to indicate which function is called
	// 2.(uint16_t *) fid: function_id that we will execute
	// 3.(uint32_t *) rpc_data_len: length of rpc_data that we will sent
	// 4.rpc_data (XXX byte): actual rpc_data

	// header_buffer: contain function_flag & fname_len
	char header_buffer[HEADER_BUFFER_SIZE];
	char *ptr = header_buffer;
	uint16_t function_flag_network = htons(RPC_CALL_FLAG);
	memcpy(ptr, &function_flag_network, sizeof(function_flag_network));
	ptr += sizeof(function_flag_network);

	uint16_t fid_network = htons(h->fid);
	memcpy(ptr, &fid_network, sizeof(fid_network));

	// rpc_data_len_buffer (include case that payload->data2_len = 0)
	uint32_t total_size;
	if (payload->data2_len == 0) {
		total_size = UINT64_SIZE;
	} else {
		total_size = UINT64_SIZE + UINT32_SIZE + payload->data2_len;
	}
	char rpc_data_len_buffer[UINT32_SIZE];
	ptr = rpc_data_len_buffer;
	uint32_t rpc_data_len_network = htonl(total_size);
	memcpy(ptr, &rpc_data_len_network, UINT32_SIZE);

	// rpc_data_buffer (need to serialize into a single buffer)
	char rpc_data_buffer[total_size];
	ptr = rpc_data_buffer;
	loadRPCDataToBuffer(payload, ptr);

	// send header_buffer to server
	int n = write(cl->sockfd, header_buffer, HEADER_BUFFER_SIZE);
	if (n < 0) {
		perror("socket");
		return NULL;
	}

	// send rpc_data_len_buffer to server
	n = write(cl->sockfd, rpc_data_len_buffer, UINT32_SIZE);
	if (n < 0) {
		perror("socket");
		return NULL;
	}

	// send rpc_data_buffer
	n = write(cl->sockfd, rpc_data_buffer, total_size);
	if (n < 0) {
		perror("socket");
		return NULL;
	}

	// read return_rpc_data_len from server
	// server return invalid rpc_data, if the return_rpc_data_len == 0
	char return_data_len_buffer[UINT32_SIZE];
	ptr = return_data_len_buffer;
	n = read(cl->sockfd, return_data_len_buffer, UINT32_SIZE);
	if (n <= 0) {
		perror("read");
		return NULL;
	}
	uint32_t return_data_len_network, return_data_len;
	memcpy(&return_data_len_network, ptr, UINT32_SIZE);
	return_data_len = ntohl(return_data_len_network);

	if (return_data_len == 0) {
		return NULL;
	}

	// read return_rpc_data from server
	rpc_data *return_data = malloc(sizeof(*return_data));
	char return_buffer[return_data_len];
	ptr = return_buffer;
	n = read(cl->sockfd, return_buffer, return_data_len);
	if (n <= 0) {
		perror("read");
		return NULL;
	}
	extractRPCDataFromBuffer(return_data, ptr, return_data_len);

    return return_data;
}

/* Cleans up client state and closes client */
void rpc_close_client(rpc_client *cl) {
	// sent flag = 0, to indicate closing socket signal
	char header_buffer[HEADER_BUFFER_SIZE];
	char *ptr = header_buffer;
	uint16_t close_flag_network = htons(RPC_CLOSE_CLIENT_FLAG);
	memcpy(ptr, &close_flag_network, sizeof(close_flag_network));
	ptr += sizeof(close_flag_network);

	// add dummy data to make buffer full
	uint16_t dummy_network = htons(0);
	memcpy(ptr, &dummy_network, sizeof(dummy_network));

	int n = write(cl->sockfd, header_buffer, HEADER_BUFFER_SIZE);
	if (n < 0) {
		perror("socket");
		free(cl);
		return;
	}

	close(cl->sockfd);
	free(cl);
}

/* load rpc_data into buffer*/
void loadRPCDataToBuffer(rpc_data *payload, char *buffer_pointer) {
	uint64_t data1_network = hton64bit(payload->data1);
	fprintf(stderr, "[client] rpc_data->data1_network: %" PRIu64 "\n", data1_network);
	memcpy(buffer_pointer, &data1_network, sizeof(data1_network));
	buffer_pointer += sizeof(data1_network);

	if (payload->data2_len != 0) {
		uint32_t data2_len_network = htonl(payload->data2_len);
		memcpy(buffer_pointer, &data2_len_network, sizeof(data2_len_network));
		buffer_pointer += sizeof(data2_len_network);

		memcpy(buffer_pointer, payload->data2, payload->data2_len);
	}
}

/* extract buffer to rpc_data*/
void extractRPCDataFromBuffer(rpc_data *payload, char *buffer_pointer, uint32_t payload_len) {
	uint64_t data1_network, data1;
	memcpy(&data1_network, buffer_pointer, sizeof(data1_network));
	data1 = n64bittoh(data1_network);
	payload->data1 = data1;
	buffer_pointer += sizeof(data1_network);

	fprintf(stderr, "payload->data1: %d\n", payload->data1);
	
	if (payload_len == RPC_DATA_NULL_DATA2_SIZE) {
		// implement NULL data2 incase no data2 in return_buffer
		payload->data2_len = 0;
		payload->data2 = NULL;
	} else {
		// extract data2_len & data2 if available in return_buffer
		uint32_t data2_len_network, data2_len;
		memcpy(&data2_len_network, buffer_pointer, sizeof(data2_len_network));
		data2_len = ntohl(data2_len_network);
		payload->data2_len = data2_len;
		buffer_pointer += sizeof(data2_len_network);

		payload->data2 = malloc(payload->data2_len);
		if (payload->data2 == NULL) {
			perror("Memory allocation failed");
		}
		memcpy(payload->data2, buffer_pointer, payload->data2_len);
	}
	fprintf(stderr, "payload->data2_len: %ld\n", payload->data2_len);
	fprintf(stderr, "payload->data2: %p\n", payload->data2);
}

/* convert 64-bit data to network byte order format */
/* inspired from https://codereview.stackexchange.com/questions/151049/endianness-conversion-in-c */
uint64_t hton64bit(uint64_t data) {
    // Check the host's byte order
    static const int32_t num = 1;
    static const uint8_t* const is_little_endian_pointer = (const uint8_t*)&num;
    if (*is_little_endian_pointer == 1) {
        return ((data & 0x00000000000000FFULL) << 56) |
               ((data & 0x000000000000FF00ULL) << 40) |
               ((data & 0x0000000000FF0000ULL) << 24) |
               ((data & 0x00000000FF000000ULL) << 8) |
               ((data & 0x000000FF00000000ULL) >> 8) |
               ((data & 0x0000FF0000000000ULL) >> 24) |
               ((data & 0x00FF000000000000ULL) >> 40) |
               ((data & 0xFF00000000000000ULL) >> 56);
    } else {
        return data;
    }
}

/* convert 64-bit data from network byte order format to host format*/
/* inspired from https://codereview.stackexchange.com/questions/151049/endianness-conversion-in-c */
uint64_t n64bittoh(uint64_t data) {
    // Check the format of this host
	int num = 1;
	if (*(char*)&num == 0) {
		// if it's a big endian system
		return data;
	} else {
		// if it's a little endian system
		return ((data & 0xFFULL) << 56) |
               ((data & 0xFF00ULL) << 40) |
               ((data & 0xFF0000ULL) << 24) |
               ((data & 0xFF000000ULL) << 8) |
               ((data & 0xFF00000000ULL) >> 8) |
               ((data & 0xFF0000000000ULL) >> 24) |
               ((data & 0xFF000000000000ULL) >> 40) |
               ((data & 0xFF00000000000000ULL) >> 56);
	}
}

/* Frees a rpc_data struct */
void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}