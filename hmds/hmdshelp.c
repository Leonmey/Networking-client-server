/*
*hmdshelp.c
*Leon Meyer
*CS3357 assignment 2
*helps the Server side
*Deals with the program arguments
*/
#include "hmds.h"

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

int wait_for_connection(int sockfd)
{
  struct sockaddr_in client_addr; // Remote IP that is connecting to us
  unsigned int addr_len = sizeof(struct sockaddr_in); // Length of the remote IP structure
  char ip_address[INET_ADDRSTRLEN]; // Buffer to store human-friendly IP address
  int connectionfd; // Socket file descriptor for the new connection

  // Wait for a new connection
  connectionfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len);

  // Make sure the connection was established successfully
  if (connectionfd == -1)
    err(EXIT_FAILURE, "%s", "Unable to accept connection");

  // Convert the connecting IP to a human-friendly form and print it
  inet_ntop(client_addr.sin_family, &client_addr.sin_addr, ip_address, sizeof(ip_address));

  syslog(LOG_INFO, "Connection accepted from %s\n", ip_address);

  // Return the socket file descriptor for the new connection
  return connectionfd;
}

struct addrinfo* get_server_sockaddr(const char* port)
{
  struct addrinfo hints;
  struct addrinfo* results;

  memset(&hints, 0, sizeof(struct addrinfo));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int retval = getaddrinfo(NULL, port, &hints, &results);
  if(retval)
  {
    syslog(LOG_ERR, "Unable to get address info");
    exit(EXIT_FAILURE);
  }

  return results;
}

void handle_connection(int connectionfd, hdb_connection* redCon, int verboseFlag)
{
  char buffer[BUFFER_SIZE];        //buffer to hold messages
  char list[BUFFER_SIZE] = "";     //Holds the list
  int bytes_read;                  //number of bytes read when receiving a message
  char* token = NULL;              //holds characters that will be tokenized
  char* username = NULL;           //username for authentication
  char* password = NULL;           //password for authentication
  char* authToken = NULL;          //authentication token
  char message[MESSAGE_SIZE];      //holds the message for authentication
  do
  {
    // Read up to 4095 bytes from the client
    bytes_read = recv(connectionfd, buffer, sizeof(buffer)-1, 0);

    // If the data was read successfully
    if (bytes_read > 0)
    {
      if(verboseFlag)
        syslog(LOG_DEBUG, "Data was read successfully");

      token = strtok(buffer,NEWLINE);
      if(verboseFlag)
        syslog(LOG_DEBUG, "Message type recieved was:%s",token);

      //find out what kind of message was sent
      if(strcmp(token,"AUTH") == 0)
      {
        token = strtok(NULL, TOK_COLON);
        username = strtok(NULL, NEWLINE);
        token = strtok(NULL, TOK_COLON);
	    password = strtok(NULL, NEWLINE);

	    //verify username and password with redis server
	    authToken = hdb_authenticate(redCon,username,password);

	    //pass is correct or creates new user
	    if(authToken != NULL)
	    {
	      syslog(LOG_INFO, "Username: %s",username);
	      if(verboseFlag)
	        syslog(LOG_INFO, "Authentication successful");

          //build message 200 for successful authentication
	      strcpy(message,"200 Authentication successful\nToken:");
	      strcat(message, authToken);
	      strcat(message, DOUBLE_NEWLINE);
	    }
	    else //pass is incorrect
        {
	      syslog(LOG_INFO, "Incorrect Password: Unauthorized");

	      //buid message 401 unauthorized
	      strcpy(message, "401 Unauthorized\n\n");
     	}

	    if(send(connectionfd, message, strlen(message), 0) == -1)
	    {
	      syslog(LOG_WARNING,"Unable to send data to client");
	      break;
        }
      }
      else if(strcmp(token,"LIST") == 0)
      {
        //decompose the message to get the the auth token
        token = strtok(NULL, TOK_COLON);
        authToken = strtok(NULL, NEWLINE);
	    token = strtok(NULL, TOK_COLON);
        token = strtok(NULL, DOUBLE_NEWLINE);

        //verify the authToken and receive the username
	    username = hdb_verify_token(redCon, authToken);

	    if(verboseFlag)
	      syslog(LOG_DEBUG, "LIST message, authenticated username:%s",username);

	    //authToken is invalid
	    if(username == NULL)
	    {
	      syslog(LOG_INFO, "Unauthorized");

	      //build message 401 unauthorized
	      strcpy(message, "401: Unauthorized\n\n");
	    }
	    else
	    {
          //authorized
	      syslog(LOG_INFO, "Receiving file list");

	      char filename[MAX_FILE_LENGTH] = "";
	      char* checksum = NULL;

          token = strtok(NULL, NEWLINE);

          //create the list of files the redis server does not have
	      while(token != NULL)
	      {
            strcpy(filename, token);
            checksum = strtok(NULL, NEWLINE);
	        token = strtok(NULL, NEWLINE);

	        if(verboseFlag)
	        {
	          syslog(LOG_INFO, "* %s, %s", filename, checksum);
	        }

            //get the checksum from the redis server
	        char* crc = hdb_file_checksum(redCon,username,filename);
	        //check if the file exists and if the checksum is correct
	        if(hdb_file_exists(redCon, username, filename ) && strcmp(checksum, crc) == 0)
	        {
	          //do nothing the file exists and has not been changed
	        }
	        else
	        {
	          //file has beeen changed or does not exist in database
	          strcat(list,NEWLINE);
	          strcat(list,filename);
	        }
          }

          //if the list is not emtpy then redis is not up to date
	      if(strcmp(list, "") != 0)
	      {
	        syslog(LOG_INFO, "Requesting files:");

	        strcpy(buffer,list);
	        token = strtok(buffer, NEWLINE);
	        while(token != NULL)
	        {
	          syslog(LOG_INFO, "* %s",token);
	          token = strtok(NULL, NEWLINE);
	        }

	        int length = strlen(list);
	        snprintf(buffer, BUFFER_SIZE, "302 Files requested\nLength:%d\n",length);
	        strcat(buffer,list);

	        if(send(connectionfd, buffer, strlen(buffer), 0) == -1)
	        {
	          syslog(LOG_WARNING,"Unable to send data to client");
              break;
	        }
	      }
	      else
	      {
            //send the no files requested message
	        strcpy(message,"204 No files requested\n\n");
	        if(send(connectionfd, message, strlen(message), 0) == -1)
	        {
	          syslog(LOG_WARNING,"Unable to send data to client");
	          break;
	        }
	      }
        }
      }
    }
  } while (bytes_read > 0);

  // Close the connection
  close(connectionfd);
}


