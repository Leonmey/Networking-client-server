/*
*hftphelp.c
*Leon Meyer
*CS3357 assignment 2
*helps the Server side
*Deals with the program arguments
*/
#include "hftpd.h"

int bind_socket(struct addrinfo* addr_list)
{
  struct addrinfo* addr;
  int sockfd;
  // Iterate over the addresses in the list; stop when we successfully bind to one
  for (addr = addr_list; addr != NULL; addr = addr->ai_next)
  {
    // Open a socket
    sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

    // Move on to the next address if we couldn't open a socket
    if (sockfd == -1)
      continue;
    // Try to bind the socket to the address/port
    if (bind(sockfd, addr->ai_addr, addr->ai_addrlen) == -1)
    {
      // If binding fails, close the socket, and move on to the next address
      close(sockfd);
      continue;
    }
    else
    {
      // Otherwise, we've bound the address to the socket, so stop processing
      break;
    }
  }
  // Free the memory allocated to the address list
  freeaddrinfo(addr_list);

  // If addr is NULL, we tried every address and weren't able to bind to any
  if (addr == NULL)
  {
    syslog(LOG_ERR, "Unable to bind to address");
    exit(EXIT_FAILURE);
  }
  else
  {
    // Otherwise, return the socket descriptor
    return sockfd;
  }
}

int create_server_socket(char* port, int verboseFlag)
{
 struct addrinfo* results = get_udp_sockaddr(NULL, port, AI_PASSIVE);
 if(verboseFlag)
    syslog(LOG_DEBUG, "Got server socket address");

 int sockfd = bind_socket(results);
 return sockfd;
}

//build the response message
message* create_response_message(ctrl_message* request, bool auth,bool term_requested)
{

  resp_msg* response = (resp_msg*)create_message();
  if(!auth)
    response->err_code = 1;
  else if(term_requested) //tells the client im exiting
    response->err_code = 2;
  else
    response->err_code = 0;

  response->length = 4;
  response->type = 0xFF;
  response->sequence = request->sequence;


  // Return the dynamically-allocated message
  return (message*)response;
}

//Create the new directories
FILE* fopen_mkdir(const char* name)
{
    char* mname = strdup(name);
    int i;
    for(i=0; mname[i]!='\0'; i++)
    {
        if(i>0 && (mname[i]=='\\' || mname[i] == '/'))
        {
            char slash = mname[i];
            mname[i]='\0';
            mkdir(mname, 0777 );
            mname[i]=slash;
        }
    }
    free(mname);
    return fopen(name, "a+");
}

//changes the expected sequence
int flip_sequence(int seq){
  if (seq == 1)
    seq = 0;
  else
    seq = 1;

  return seq;
}
