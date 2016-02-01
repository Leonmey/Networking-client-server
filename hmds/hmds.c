/*
*hmds.c
*Leon Meyer
*CS3357 assignment 2
*Server side of the connection
*Deals with the program arguments
*/
#include "hmds.h"

int main(int argc, char* argv[])
{
openlog("hmds", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
int c;                              //choice for arguments
static int verboseFlag = 0;         //flag for the verbose option
char* hostname = DEFAULT_HOSTNAME;  //redis hostname
char* port = DEFAULT_PORT;          //port to conenct with client

while(1)
{
  static struct option longOptions[] =
  {
    {"verbose", no_argument, &verboseFlag, 1},
    {"hostname", required_argument, 0, 'r'},
    {"port", required_argument, 0, 'p'},
    {0, 0, 0, 0}
  };

  int optionIndex = 0;
  c = getopt_long(argc, argv, "vs:p:d:", longOptions, &optionIndex);

  if(c == -1)
    break;

  switch (c)
  {
    case 'v':
      verboseFlag = 1;
      break;

    case 'r':
      hostname = optarg;
      break;

    case 'p':
      port = optarg;
      break;

    case '?':
      syslog(LOG_ERR, "Invalid argument");
      exit(EXIT_FAILURE);
      break;
  }
}

  //connect to the redis server
  hdb_connection* redCon = hdb_connect(hostname);
  if(redCon == NULL)
  {
    syslog(LOG_ERR, "Could not connect to redis server");
    exit(EXIT_FAILURE);
  }
  if(verboseFlag)
    syslog(LOG_DEBUG, "Connected to redis server");

  // We want to listen on the port specified on the command line
  struct addrinfo* results = get_server_sockaddr(port);
  if(verboseFlag)
    syslog(LOG_DEBUG, "Got server socket address");

  // Create a listening socket
  int sockfd = bind_socket(results);
  if(verboseFlag)
    syslog(LOG_DEBUG, "Socket bound");

  // Start listening on the socket
  if (listen(sockfd, MESSAGE_QUEUE) == -1)
  {
    syslog(LOG_ERR, "Unable to listen on socket");
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO, "Server listening on port %s",port);

  syslog(LOG_INFO, "To terminate server press Ctrl+C");
  while(1)
  {
    // Wait for a connection then handle it
    int connectionfd = wait_for_connection(sockfd);
    handle_connection(connectionfd, redCon, verboseFlag);
    syslog(LOG_DEBUG, "Connection handled");
  }

  //close(connectionfd)

  // Close the socket and exit
  close(sockfd);
  exit(EXIT_SUCCESS);
}

