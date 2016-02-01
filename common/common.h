
/*
*common.h
*Leon Meyer
*CS3357 assignment 3
*Common server client declarations
*/

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

// Maximum amount of data per UDP datagram
#define BUFFER_MAX          1472
#define DATA_MAX            1468
#define BUFFER_SIZE         4096

typedef struct
{
 int length;
 uint8_t buffer[BUFFER_MAX-4];
} message;

typedef struct
{
 int length;
 uint8_t type;
 uint8_t sequence;
 uint16_t filename_len;
 uint32_t file_size;
 uint32_t checksum;
 uint32_t token[4];
 uint8_t filename[1440];
} ctrl_message;

typedef struct
{
 int length;
 uint8_t type;
 uint8_t sequence;
 uint16_t err_code;
 uint8_t buffer[BUFFER_MAX-4];
} resp_msg;

typedef struct
{
 int length;
 uint8_t type;
 uint8_t sequence;
 uint16_t data_length;
 uint8_t data[BUFFER_MAX - 4];
} data_message;

typedef struct
{
 struct sockaddr_in addr;
 socklen_t addr_len;
 char friendly_ip[INET_ADDRSTRLEN];
} host;

struct addrinfo* get_udp_sockaddr(const char* node, const char* port, int flags);
message* receive_message(int sockfd, host* source);
message* create_message();
int send_message(int sockfd, message* msg, host* dest);
