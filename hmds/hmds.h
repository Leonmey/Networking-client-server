#include <netdb.h>
#include <hdb.h>
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

#define MAX_FILE_LENGTH     100
#define MESSAGE_SIZE        50
#define BUFFER_SIZE         4096
#define DEFAULT_PORT        "9000"
#define DEFAULT_HOSTNAME    "localhost"
#define MESSAGE_QUEUE       25
#define TOK_COLON           ":"
#define DOUBLE_NEWLINE      "\n\n"
#define NEWLINE             "\n"

int bind_socket(struct addrinfo* addr_list);
int wait_for_connection(int sockfd);
struct addrinfo* get_server_sockaddr(const char* port);
void handle_connection(int connectionfd, hdb_connection* redCon, int verboseFlag);

