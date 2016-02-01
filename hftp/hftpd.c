/*
*hftpd.c
*Leon Meyer
*CS3357 assignment 3
*Server side of the connection
*/
#include "hftpd.h"

static bool term_requested = false;

//handles CTRL-C
void term_handler(int signal)
{
 printf("\nTermination requested...\n");
 term_requested = true;
}

int main(int argc, char* argv[])
{
signal(SIGINT,term_handler);
openlog("hftpd", LOG_PERROR | LOG_PID | LOG_NDELAY, LOG_USER);
int c;                                              //choice for arguments
FILE* fp = NULL;                                           //file to open
int exp_seq = 0;                                    //first expected sequence
char* username;                                     //username set by redis verify auth
ctrl_message* request;                              // Client's request message
data_message* data_msg;                             // clients data message
message* response;                                  // Server response message
host client;                                        // Source of the message received
uint32_t filesize = 0;                              //size of the file
uint8_t cur_file_size = 0;                          //amount of data that has been sent                                 //terminate time wait flag
int retval;                                         //return value from poll
bool auth = false;                                  //set authentication to false
char path[BUFFER_SIZE] = "";                        //path for file names;
static int verboseFlag = 0;                         //flag for the verbose option
char* hostname = DEFAULT_HOSTNAME;                  //redis hostname
char* port = DEFAULT_PORT;                          //port to conenct with client
char* dir = DEFAULT_DIR;                            //root dir, defaults to /tmp/hftpd
char* timewait = DEFAULT_TIMEWAIT;                   //number of seconds to spend in time_wait, default 10

while(1)
{
  static struct option longOptions[] =
  {
    {"verbose", no_argument, &verboseFlag, 1},
    {"redis", required_argument, 0, 'r'},
    {"port", required_argument, 0, 'p'},
    {"dir", required_argument, 0, 'd'},
    {"timewait", required_argument, 0, 't'},
    {0, 0, 0, 0}
  };

  int optionIndex = 0;
  c = getopt_long(argc, argv, "vr:p:d:t:", longOptions, &optionIndex);

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

    case 'd':
      dir = optarg;
      break;

    case 't':
      timewait = optarg;
      break;

    case '?':
      syslog(LOG_ERR, "Invalid argument");
      exit(EXIT_FAILURE);
      break;
  }
}
  //allocate memory for checksum and records
  char *checksum =malloc(sizeof(char*));
  struct hdb_record* record = malloc(sizeof(hdb_record*));

  //connect to the redis server
  hdb_connection* redCon = hdb_connect(hostname);
  if(redCon == NULL)
  {
    syslog(LOG_ERR, "Could not connect to redis server");
    exit(EXIT_FAILURE);
  }
  if(verboseFlag)
    syslog(LOG_DEBUG, "Connected to redis server");

    // Create a listening socket
  int sockfd = create_server_socket(port, verboseFlag);

//loop until CTRL-C
while(!term_requested){

  //Poll for the data
  struct pollfd fd = {
  .fd = sockfd,
  .events = POLLIN
  };
  retval = poll(&fd, 1, atoi(timewait));

  if (retval == 1 && fd.revents == POLLIN)
  {
    // Read the request message and generate the response
    request = (ctrl_message*)receive_message(sockfd, &client);

    if(request->sequence == (uint8_t)exp_seq){

      //start message
      if(request->type == (uint8_t)CTRL_START){
        syslog(LOG_DEBUG,"Initialize MESSAGE");
        char token[17];
        memcpy(token,request->token,16);
        token[16] = '\0';
        username = hdb_verify_token(redCon, token);
        if(username == NULL){
          syslog(LOG_ERR, "Authentication token incorrect");
          auth = false;
          free(username);
        }
        else{
        //authentication successful
          auth = true;
          if(verboseFlag)
            syslog(LOG_DEBUG, "Authentication success");

          if(fp != NULL){
            fclose(fp);
            syslog(LOG_DEBUG, "File Closed");
          }

          snprintf(path, sizeof(path), "%s%s%s%s%s", dir,F_SLASH,username,F_SLASH,(char*)request->filename);

          if(verboseFlag)
            syslog(LOG_DEBUG, "file path: %s",path);

          fp = fopen(path,"a+");
          if(fp == NULL){
            FILE* p = fopen_mkdir(path);
            fp = p;
          }

          if(verboseFlag)
            syslog(LOG_DEBUG, "File opened: %s",(char*)request->filename);

          filesize = request->file_size;
          record->username = username;
          record->filename = (char*)request->filename;

          sprintf(checksum,"%X",request->checksum);
          record->checksum = checksum;

          free(username);

        }

        response = create_response_message(request, auth, term_requested);
        if(send_message(sockfd, response, &client) == -1)
            syslog(LOG_ERR,"Failed to send response");
        free(response);

        exp_seq = flip_sequence(exp_seq);
      }
      //TERMINATE CTRL MESSAGE
      else if(request->type == CTRL_TERMINATE){
        if (verboseFlag)
          syslog(LOG_DEBUG,"TERMINATE MESSAGE");

        fclose(fp);
        fp = NULL;

        response = create_response_message(request, auth, term_requested);
        if(send_message(sockfd, response, &client) == -1)
          syslog(LOG_ERR, "Send message error");
        free(response);

        syslog(LOG_INFO, "Transfer terminated.");

        auth = false;
        exp_seq = 0;

      }
      //DATA MESSAGE
      else if(request->type == DATA_MESSAGE){
        if (verboseFlag)
          syslog(LOG_DEBUG,"DATA MESSAGE");

        response = create_response_message(request, auth, term_requested);
        if(send_message(sockfd, response, &client) == -1)
          syslog(LOG_ERR, "Send message error");
        free(response);

        exp_seq = flip_sequence(exp_seq);

        data_msg = (data_message*)request;

        char* data = 0;
        data = (char*)malloc(data_msg->data_length);
        memcpy(data,data_msg->data,data_msg->data_length);

        if(fp == NULL){
            syslog(LOG_DEBUG, "could not open file");
        }
        //append data to the file
        fwrite(data,1,data_msg->data_length,fp);
        cur_file_size += (uint8_t)data_msg->data_length;

        //update redis if the entire file has been uploaded
        if(cur_file_size == (uint8_t)filesize){
          hdb_store_file(redCon, record);
          syslog(LOG_INFO, "File Updated");
          cur_file_size = 0;
        }
        free(data);
      }
    }
    else if(request->sequence != exp_seq){

      response = create_response_message(request, auth, term_requested);
      if(send_message(sockfd, response, &client) == -1)
          syslog(LOG_ERR, "Send message error");
      free(response);
    }
    free(request);
  }
  //if CTRL-C was pressed exit
  else if(term_requested){
    break;
  }
  else
  {
    if(fp != NULL)
      fclose(fp);

      fp = NULL;
      auth = false;
      exp_seq = 0;
      syslog(LOG_INFO, "Waiting for Control Message");
  }
}

  free(checksum);
  free(record);

  hdb_disconnect(redCon);
  // Close the socket and exit
  close(sockfd);
  exit(EXIT_SUCCESS);
}

