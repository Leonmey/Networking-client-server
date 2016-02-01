/*
*client.c
*Leon Meyer
*CS3357 assignment 2
*Client side of the connection
*Deals with the program arguments
*/
#include "client.h"

int main(int argc, char *argv[])
{
openlog("client", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
int c,i;                                      //choice and counter for arguments
int retval;                                 //return value for timeout
int timeout_count =0,timeout_count2 = 0;    //counter to end after 3 timeouts
static int  verboseFlag = 0;                //flag for verbose mode
char* serverHostname = DEFAULT_HOSTNAME;    //default is localhost
char* port = DEFAULT_PORT;                  //default is 9000
char* dir = DEFAULT_DIR;                    //default is ~/hooli
char* fserver = DEFAULT_HOSTNAME;           //default is localhost
char* fport = DEFAULT_FPORT;                //default is 10000
char* username = NULL;                      //required argument given by user
char* password = NULL;                      //required argument given by user
char* authToken = NULL;                     //holds the authentication code from the server
bool  exits = false;                         //flag to exit gracefully

while(1)
{
  static struct option longOptions[] =
  {
    {"verbose", no_argument, &verboseFlag, 1},
    {"server", required_argument, 0, 's'},
    {"port", required_argument, 0, 'p'},
    {"dir", required_argument, 0, 'd'},
    {"fserver", required_argument, 0, 'f'},
    {"fport", required_argument, 0, 'o'},
    {0, 0, 0, 0}
  };

  int optionIndex = 0;
  c = getopt_long(argc, argv, "vs:p:d:f:o:", longOptions, &optionIndex);

  if(c == -1)
    break;

  switch (c)
  {
    case 'v':
      verboseFlag = 1;
      break;

    case 's':
      serverHostname = optarg;
      break;

    case 'p':
      port = optarg;
      break;

    case 'd':
      dir = optarg;
      break;

    case 'f':
      fserver = optarg;
      break;

    case 'o':
      fport = optarg;
      break;

    case '?':
      exit(EXIT_FAILURE);
      break;
  }
}

i = optind;

if (i +1 < argc)
{
  username = argv[i++];
  password = argv[i];
}
else
{
//username or password missing, print error
  syslog(LOG_ERR, "Username or Password missing");
  exit(EXIT_FAILURE);
}
if(verboseFlag)
  syslog(LOG_DEBUG,"Username:%s",username);

//scan the directory and return a linked list of files/checksums
  struct hfs_entry* list = hfs_get_files(dir);
  if(list == NULL)
    syslog(LOG_WARNING, "could not scan directory");

  // Print the scanned files
  hfs_entry* cur = list;
  if (verboseFlag) {
    cur = list;
    while (cur != NULL) {
      syslog(LOG_DEBUG, "* %s (%X)\n", cur->rel_path, cur->crc32);
      cur = cur->next;
    }
  }

  //get the socket address and then open connection with server
  struct addrinfo* results = get_sockaddr(serverHostname, port);
  int sockfd = open_connection(results);

  //Send the authenticate message
  if (verboseFlag) {
    syslog(LOG_DEBUG, "Sending Credentials");
  }
  handle_auth_request(sockfd,username,password);

  // Read the response
  struct hmdp_response* auth_response = hmdp_read_response(sockfd);
  if (auth_response == NULL) {
    syslog(LOG_ERR, "Failed to read response");
  }

  // Authenticate if the code is 200
  int response_code = auth_response->code;
  if (response_code != AUTH_SUCCESS) {
    // Authentication failed
    syslog(LOG_ERR, "Username or Password Incorrect, Unauthorized\n");
    exit(EXIT_FAILURE);
  }
  syslog(LOG_INFO, "Authenticated");

  // Extract the Token from the Response
  authToken = hmdp_header_get(auth_response->headers, "Token");

  // Create and send the LIST request and listen for a response
  handle_list_request(authToken, sockfd, list);

  // Read the response
  struct hmdp_response* list_response = hmdp_read_response(sockfd);
  if (list_response == NULL) {
    syslog(LOG_ERR, "Failed to read list response");
    exit(EXIT_FAILURE);
  }

  //check if the list response had files requested
  response_code = list_response->code;
  if (response_code == NO_FILE_REQ) {
    syslog(LOG_ERR, "No files requested\n");
    exit(EXIT_FAILURE);
  } else if (response_code == FILE_REQ) {
    // Print a list of the files and count the amount
    syslog(LOG_INFO, "Server requested the following files:\n%s\n", list_response->body);
  }

  int count;
  char* temp = list_response->body;
  for(count = 0;temp[count];temp[count]=='\n'
      ? count++ : *temp++);
  count++;

  if(verboseFlag)
        syslog(LOG_INFO, "Number of files requested: %d", count);
  // Close the socket
  close(sockfd);

 // Open connection with hftpd
  host server;
  int hftpd_sockfd = create_client_socket(fserver, fport, &server);
  if (verboseFlag)
    syslog(LOG_DEBUG, "HFTPD Socket created");

  // Create a control message to use
  int sequence = 0;
  int file_number = 0;
  char* tok = strtok(list_response->body, "\n");

//CONTROL MESSAGE STARTS

  while (tok != NULL) {
    int crc = 0;
    char* file_path = "";

    hfs_entry* reply = hfs_get_files(dir);
    hfs_entry* cur = reply;

    while (cur != NULL) {
      if (strstr(cur->abs_path, tok) != NULL) {
        crc = cur->crc32;
	    file_path = cur->abs_path;
        break;
      }
      cur = cur->next;
    }

    // For each file, get the size of the file
    FILE *f;
    long file_size = 0;
    f = fopen(file_path, "rb");

    if (f == NULL) {
      syslog(LOG_ERR,"Cannot open file");
      exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    fclose(f);

    // Create the message and initialize contents
    message* control_message = create_control_msg(1, sequence,
				strlen(tok), (int)file_size, (int)crc, authToken, tok);


    syslog(LOG_INFO, "Sending control message for file %d", ++file_number);
    if (send_message(hftpd_sockfd, control_message, &server) == -1) {

      // Error sending, fail
      close(hftpd_sockfd);
      syslog(LOG_ERR, "Failed to send ctrl message\n");
      exit(EXIT_FAILURE);
    }

    // Free the control message
    //free(control_message);


    struct pollfd fd = {
    .fd = sockfd,
    .events = POLLIN
    };
    retval = poll(&fd, 1, 10000);

    if (retval == 1 && fd.revents == POLLIN){

    // Check response message to see if token was valid
    syslog(LOG_INFO, "Receiving response from server");
    resp_msg* response = (resp_msg*)receive_message(hftpd_sockfd, &server);

    if (response->err_code == 1) {
      // Invalid code, so reject
      close(hftpd_sockfd);
      syslog(LOG_ERR, "Failed to verify the token");
      exit(EXIT_FAILURE);
    }
    else{

      //free(response);
      if (sequence == 1)
        sequence = 0;
      else
        sequence = 1;

      //DATA MESSAGE

      // Token verified, prepare to send data chunks
      bool send_complete = false;
      int data_remaining = (int)file_size;
      int data_length = 0;
      int cont = 0;
      char* file_body = 0;

      // Get the body of the file in a character pointer
      FILE *fp = fopen(file_path, "rb");

      if (fp) {
        file_body = (char *)malloc(file_size);

	    if (file_body) {
	      fread(file_body, 1, file_size, fp);
	    }
      }
      fclose(fp);

      while (!send_complete) {

	    char* data = 0;

	    if (file_size == 0) {
	      // Empty files
	      syslog(LOG_ERR, "Files are empty");
	      break;
	    }
	    else if (data_remaining > DATA_MAX) {
	      // Data needs to be segmented into multiple messages
	      data_remaining -= DATA_MAX;
	      data_length = DATA_MAX;

  	      // Get the data from the filename equivalent to data_length
	      data = (char *)malloc(DATA_MAX);
	      memcpy(data, &file_body[cont], data_length);

	      cont += data_length;

        }
        else if (data_remaining <= DATA_MAX){
  	      // Remaining data can be sent in one chunk
	      data_length = data_remaining;
	      data = (char *)malloc(data_length);
	      memcpy(data, &file_body[cont], data_length);

  	      send_complete = true;
	    }
	    syslog(LOG_DEBUG,"HERE as well?");
	    //free(data);
	    syslog(LOG_DEBUG,"HERE as well2?");
	    //free(file_body);

	    // Create the message
        message* data_message = create_data_msg(3, sequence, data_length, data);
        if(verboseFlag)
          syslog(LOG_DEBUG, "Sending data chunk");

	    // Send the message
	    if (send_message(hftpd_sockfd, data_message, &server) == -1) {
	      //Error sending, fail
	      syslog(LOG_ERR, "Failed to send data message");
	      exits= true;
	    }

        cur = reply;
        hfs_entry* freecur;

        while (cur != NULL) {
          freecur = cur;
          cur = cur->next;
          free(freecur->abs_path);
          free(freecur);
        }
        syslog(LOG_DEBUG,"maybe HERE?");
        // Free the message
	    //free(data_message);

        if(verboseFlag)
          syslog(LOG_DEBUG, "Receiving server ack");

        struct pollfd fd2 = {
        .fd = sockfd,
        .events = POLLIN
        };
        retval = poll(&fd2, 1, 10000);

        if (retval == 1 && fd2.revents == POLLIN){

        if(!exits){
          // Check the response message
          resp_msg* response_ack = (resp_msg*)receive_message(hftpd_sockfd, &server);

          if (response_ack->sequence != sequence) {
            // Did not receive the proper acknowledgement
            syslog(LOG_ERR, "Failed to receive proper ack");
            exits = true;
          }
          else if(response_ack->err_code == 2){
            syslog(LOG_ERR, "Server terminated. Exiting");
            exits = true;
          }
            syslog(LOG_DEBUG,"HERE?");
          //free(response_ack);
        }
        if (sequence == 1)
            sequence = 0;
        else
            sequence = 1;

        }
        else{
            send_complete = false;
            timeout_count2 ++;
            if(timeout_count2 == 3){
              exits = true;
              send_complete = true;
            }
        }
        // Received the proper ack, continue looping
      }
    }
    syslog(LOG_INFO, "File upload complete for file #%d", file_number);
    tok = strtok(NULL, "\n");

    timeout_count = 0;
    }
    else{
        syslog(LOG_INFO,"Timed out, resending message");
        timeout_count ++;
        if (timeout_count == 3){
          exits = true;
          syslog(LOG_ERR, "Too many timeouts, exiting.");
        }
    }
    //check if exit flag is set to close gracefully
    if(exits)
      break;
  }

  // Free the list response
  hmdp_free_response(list_response);

  // Free the auth response
  hmdp_free_response(auth_response);
  if(exits)
    syslog(LOG_INFO, "File transfer incomplete");
  else
    syslog(LOG_INFO, "File transfer complete!");

  while(exits == false){
    // Files have completed sending, send a type 2 control message
    char* empty = "";
    message* control_message = create_control_msg(2,sequence,0,0,0,empty, empty);

    syslog(LOG_INFO, "Sending terminate message");
    if (send_message(hftpd_sockfd, control_message, &server) == -1) {
      // Error sending
      syslog(LOG_ERR, "Failed to send termination message");
      exits = true;
    }

    // Free the message
    //free(control_message);

    struct pollfd fd3 = {
    .fd = sockfd,
    .events = POLLIN
    };
    retval = poll(&fd3, 1, 10000);

    if (retval == 1 && fd3.revents == POLLIN){
    // Wait for the acknowledgement
    syslog(LOG_INFO, "Waiting for termination acknowledgement");
    resp_msg* termination = (resp_msg*)receive_message(hftpd_sockfd, &server);

    if (termination->sequence == sequence || termination->err_code == 2) {
      exits = true;
    }
    else{
      syslog(LOG_ERR, "Termincation not ack... Resending");
    if(sequence == 1)
        sequence = 0;
    else
        sequence = 1;
    }

    // Free the response
    free(termination);
    }
}

//**************************//
// End of control/data message block
//**************************//

  syslog(LOG_INFO, "Connection Terminated\n");
  close(hftpd_sockfd);
  closelog();
  return 0;
}
