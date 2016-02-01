/*
*hftpd.h
*Leon Meyer
*CS3357 assignment 3
*Header file for hftpd.c
*Includes, definitions, and prototypes
*/
#include "../common/common.h"
#include <netdb.h>
#include <hdb.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <arpa/inet.h>
#include <err.h>
#include <poll.h>
#include <signal.h>

#define DATA_MESSAGE        3
#define CTRL_TERMINATE      2
#define CTRL_START          1
#define DEFAULT_TIMEWAIT    "10000"
#define DEFAULT_DIR         "/tmp/hftpd"
#define DEFAULT_PORT        "10000"
#define DEFAULT_HOSTNAME    "localhost"
#define F_SLASH             "/"




int bind_socket(struct addrinfo* addr_list);
int create_server_socket(char* port, int verboseFlag);
FILE* fopen_mkdir(const char* name);
message* create_response_message(ctrl_message* request, bool auth, bool term_requested);
void term_handler(int signal);
int flip_sequence(int seq);

