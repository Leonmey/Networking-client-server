/*
*client.h
*Leon Meyer
*CS3357 assignment 3
*Header file for client.c
*Includes, definitions, and prototypes
*/
#include "../common/common.h"
#include <netdb.h>
#include <hfs.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <err.h>
#include <hmdp.h>
#include <inttypes.h>
#include <stdbool.h>
#include <poll.h>

#define DEFAULT_FPORT       "10000"
#define DEFAULT_DIR         "~/hooli"
#define DEFAULT_PORT        "9000"
#define DEFAULT_HOSTNAME    "localhost"
#define AUTH_SUCCESS        200
#define FILE_REQ            302
#define NO_FILE_REQ         204


struct addrinfo* get_sockaddr(const char* hostname, const char* port);
int open_connection(struct addrinfo* addr_list);
int create_client_socket(char* hostname, char* port, host* server);
void handle_auth_request(int sockfd, char * user, char * pass);
void handle_list_request(char* token, int sockfd, hfs_entry *root);
message* create_control_msg(int type, int sequence, int filename_len, int file_size,
                                        int checksum, char* token, char* filename);
message* create_data_msg(int type, int sequence, int data_length, char* data);
