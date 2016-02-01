/*
*clienthelp.c
*Leon Meyer
*CS3357 assignment 2
*Set of functions that help client.c
*/
#include "client.h"

struct addrinfo* get_sockaddr(const char* hostname, const char* port)
{
  struct addrinfo hints;
  struct addrinfo* results;

  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;


  int retval = getaddrinfo(hostname, port, &hints, &results);

  if(retval)
    errx(EXIT_FAILURE, "%s", gai_strerror(retval));

  return results;
}

int open_connection(struct addrinfo* addr_list)
{
  struct addrinfo* addr;
  int sockfd;
  //Iterate through each addrinfo in the list; stop when we successfully
  // connect to one
  for (addr = addr_list; addr != NULL; addr = addr->ai_next)
  {
    // Open a socket
    sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    // Try the next address if we couldn't open a socket
    if (sockfd == -1)
      continue;
    // Stop iterating if we're able to connect to the server
    if (connect(sockfd, addr->ai_addr, addr->ai_addrlen) != -1)
      break;
 }
  // Free the memory allocated to the addrinfo list
  freeaddrinfo(addr_list);

 // If addr is NULL, we tried every addrinfo and weren't able to connect to any
 if (addr == NULL)
   err(EXIT_FAILURE, "%s", "Unable to connect");
 else
   return sockfd;
}

int create_client_socket(char* hostname, char* port, host* server) {
  int sockfd;
  struct addrinfo* addr;
  struct addrinfo* results = get_udp_sockaddr(hostname, port, 0);

  // Iterate through each addrinfo in the list, stop on socket creation
  for (addr = results; addr != NULL; addr = addr->ai_next) {
    // Open a socket
    sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

    // Try the next address if we can't open a socket
    if (sockfd == -1)
      continue;

    // Copy server address and length to the out parameter 'server'
    memcpy(&server->addr, addr->ai_addr, addr->ai_addrlen);
    memcpy(&server->addr_len, &addr->ai_addrlen, sizeof(addr->ai_addrlen));

    // Creation successful, stop loop
    break;
  }

  // Free memory
  freeaddrinfo(results);

  // If we failed to create a socket after loop exists
  if (addr == NULL) {
    perror("Unable to create socket");
    exit(EXIT_FAILURE);
  } else {
    // Otherwise, return the socket descriptor
    return sockfd;
  }
}

void handle_auth_request(int sockfd, char *user, char *pass) {

  // Create the request
  struct hmdp_request *auth_request = hmdp_create_auth_request(user, pass);

  // Send the request
  if (hmdp_send_request(auth_request, sockfd) != 0) {
    err(EXIT_FAILURE, "%s", "Unable to send auth request\n");
  }

  // Free the request
  hmdp_free_request(auth_request);
}

void handle_list_request(char* token, int sockfd, hfs_entry *root) {

  hfs_entry *freed;

  char list_body[BUFFER_MAX] = "";
  hfs_entry *cur = root;

  while (cur != NULL) {
    strcat(list_body, cur->rel_path);
    strcat(list_body, "\n");
    char crc32[32];
    sprintf(crc32, "%X", cur->crc32);
    strcat(list_body, crc32);

    if (cur->next != NULL) {
      strcat(list_body, "\n");
    }
    // Free the memory
    freed = cur;
    cur = cur->next;
    free(freed->abs_path);
    free(freed);
  }
  // Create the LIST request
  struct hmdp_request* list_request = hmdp_create_list_request(token, list_body);

  // Send the LIST request
  if (hmdp_send_request(list_request, sockfd) != 0) {
    err(EXIT_FAILURE, "%s", "Unable to send list request!\n");
  }

  // Free the request
  hmdp_free_request(list_request);
}

message* create_control_msg(int type, int sequence, int filename_len, int file_size,
					int checksum, char* token, char* filename)
{
  ctrl_message* cmsg = (ctrl_message*)create_message();
  cmsg->type = (uint8_t)type;
  cmsg->sequence = (uint8_t)sequence;
  cmsg->filename_len = (uint16_t)filename_len;
  cmsg->file_size = (uint32_t)file_size;
  cmsg->checksum = (uint32_t)checksum;
  cmsg->length = DATA_MAX;

  memcpy(cmsg->token,token,sizeof(cmsg->token));
  memcpy(cmsg->filename,filename,sizeof(cmsg->filename));

  cmsg->length = sizeof(cmsg->type) + sizeof(cmsg->sequence) + sizeof(cmsg->filename_len) +
		   sizeof(cmsg->file_size) + sizeof(cmsg->checksum) + sizeof(cmsg->token) + sizeof(cmsg->filename);

  return (message*)cmsg;
}

message* create_data_msg(int type, int sequence, int data_length, char* data)
{
  data_message* dmsg = (data_message*)create_message();
  dmsg->type = (uint8_t)type;
  dmsg->sequence = (uint8_t)sequence;
  dmsg->data_length = (uint16_t)data_length;
  memcpy(dmsg->data,data,sizeof(dmsg->data));

  dmsg->length = sizeof(dmsg->type) + sizeof(dmsg->sequence) + sizeof(dmsg->data_length) +
			sizeof(dmsg->data);

  return (message*)dmsg;
}

