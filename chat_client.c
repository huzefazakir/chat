// Client
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>


#define SERVER_IPV4_ADDR "127.0.0.1"
#define SERVER_LISTEN_PORT 9000
#define TRUE  1
#define FALSE 0

#define FAILURE -1
#define SUCCESS  0
#define MAX_BUFFER 256
#define MAX_USER_SIZE 20
#define MAX_RETRIES 10

static int client_socket = -1;
static char server_ip_address[50];
static char username[MAX_USER_SIZE];
static uint16_t server_port = 0;
FILE *fp = NULL;

static void shut_down(int code)
{
  fprintf(stderr,"Shutdown client\n");
  
  if(client_socket > -1)
    close(client_socket);

  fclose(fp);
  exit(code);
}


void handle_signal_action(int sig_number)
{
  if (sig_number == SIGINT) 
  {
    fprintf(stderr,"SIGINT was caught!\n");
    shut_down(SUCCESS);
  }

  else if (sig_number == SIGPIPE) 
  {
    fprintf(stderr,"SIGPIPE was caught!\n");
    shut_down(SUCCESS);
  }
}


static int setup_signals()
{
  struct sigaction sa;
  sa.sa_handler = handle_signal_action;

  if (sigaction(SIGINT, &sa, 0) != 0) 
  {
    perror("unable to register handler for SIGINT");
    return FAILURE;
  }

  // ignore sigpipe, handled seperately
  signal(SIGPIPE, SIG_IGN);
  return SUCCESS;
}

/*static int connect_client(struct sockaddr_in *server_addr)
{
  if (server_addr == NULL) return FAILURE;

  int retry = 0;
  while (connect(client_socket, (struct sockaddr *)server_addr, sizeof(struct sockaddr)) != 0) 
  {
    if(retry > MAX_RETRIES)
    {
      fprintf(stderr,"unable to connect to server\n");
      close(client_socket);
      return FAILURE;
    }

    retry = retry + 1;
    perror("unable to connect to server, retrying");
    sleep(5);
  }

  return SUCCESS;
}*/

static void print_help(char *argv[])
{
  fprintf(stderr,"usage: %s --host <ip address/hostname> --port <port no> --handle <username>\n", argv[0]);
  exit(SUCCESS);
}

static void parse_args(int argc, char *argv[])
{

  int opt = 0;

  int ip_flag = FALSE;
  int port_flag = FALSE;
  int user_flag = FALSE;

  int port = 0;

  while(TRUE)
  {
    int option_index = 0;

    static struct option long_options[] = {
      {"help",           no_argument,         0,  'h'},
      {"host",           required_argument,   0,  'i'},
      {"port",           required_argument,   0,  'p'},
      {"handle",         required_argument,   0,  'u'},
      {"connect",        no_argument,         0,  'u'},
      {0,                0,                   0,   0 }
    };

    opt = getopt_long(argc, argv, "chi:p:u:", long_options, &option_index);

    if (opt == -1)
      break;

    switch(opt)
    {
      case 0 : if (long_options[option_index].flag != 0)
                 break;
               printf ("option %s", long_options[option_index].name);
               if (optarg)
                 printf (" with arg %s", optarg);
               printf ("\n");
               break;

      case 'h': print_help(argv);
                break;

      case 'i': strncpy(server_ip_address, optarg, sizeof(server_ip_address));
                ip_flag = TRUE;
                break;

      case 'p': port = atoi(optarg);
                port_flag = TRUE;
                break;

      case 'u': strncpy(username, optarg, sizeof(username));
                user_flag = TRUE;
                break;
      case 'c': break;

      default: print_help(argv);

    }

  }

  if(ip_flag == FALSE || port_flag == FALSE || user_flag == FALSE)
    print_help(argv);

  if (strlen(username) == 0 || username[0] == ' ')
  {
    fprintf(stderr, "please enter a valid username\n");
    print_help(argv);
  }

  if(port > 65535)
  {
    fprintf(stderr,"Invalid port number: %d\n", port);
    print_help(argv);
  }

  server_port = (uint16_t) port;

}

int main(int argc, char *argv[])
{
  memset(server_ip_address, 0, sizeof(server_ip_address));
  memset(username, 0, sizeof(username));

  parse_args(argc, argv);

  fprintf(stderr,"server_ip_address: %s, server_port: %u, username: %s\n", server_ip_address, server_port, username);

  fp = fopen(username, "w"); 

  if(!fp)
  {
    fprintf(stderr, "unable to open username file\n");
    exit(FAILURE);
  }

  if (setup_signals() < 0)
    exit(FAILURE);

  struct addrinfo hints, *servinfo, *p;
  int rv = -1;
  //char s[INET_ADDRSTRLEN];

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char str_port[6];
  memset(str_port, 0, sizeof(str_port));
  sprintf(str_port, "%d", server_port);

  if ((rv = getaddrinfo(server_ip_address, str_port, &hints, &servinfo)) != 0) 
  {
    fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(rv));
    exit(FAILURE);
  }

  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) 
  {
    if ((client_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
    {
      perror("client: socket");
      continue;
    }

    if (connect(client_socket, p->ai_addr, p->ai_addrlen) == -1) 
    {
      close(client_socket);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL) 
  {
    fprintf(stderr,"client: failed to connect\n");
    exit(FAILURE);
  }

  //inet_ntop(p->ai_family, getnetbyaddr((struct sockaddr *)p->ai_addr), s, sizeof(s));
  //fprintf(stderr,"client: connected to %s\n", s);

  freeaddrinfo(servinfo);


  /* Set nonblock for stdin. */
  int flag = fcntl(STDIN_FILENO, F_GETFL, 0);
  flag |= O_NONBLOCK;
  fcntl(STDIN_FILENO, F_SETFL, flag);


  fd_set read_fds;
  fd_set except_fds;

  while(TRUE)
  {

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(client_socket, &read_fds);

    FD_ZERO(&except_fds);
    FD_SET(STDIN_FILENO, &except_fds);
    FD_SET(client_socket, &except_fds);

    int maxfd = client_socket;

    int activity = select(maxfd + 1, &read_fds, NULL, &except_fds, NULL);

    if (activity  == 0 || activity == -1)
    {
      perror("select failure");
      shut_down(FAILURE);
    }

    if (FD_ISSET(STDIN_FILENO, &read_fds)) 
    {
      // read message from sttdin and send to the server
      char read_buffer[MAX_BUFFER];
      memset(read_buffer, 0, MAX_BUFFER);

      ssize_t read_count = 0;
      ssize_t total_read = 0;

      do 
      {
        read_count = read(STDIN_FILENO, read_buffer, MAX_BUFFER);

        if (read_count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) 
        {
          perror("unable to read");
          return -1;
        }

        else if (read_count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) 
          break;

        else if (read_count > 0) 
        {
          total_read += read_count;

          if (total_read > MAX_BUFFER) 
          {
            fprintf(stderr,"Message too large to send\n");
            fflush(STDIN_FILENO);
            break;
          }
        }
      } while (read_count > 0 && total_read < MAX_BUFFER);

      if(total_read < MAX_BUFFER && total_read > 0)
      {
        size_t len = strlen(read_buffer);
        //fprintf(stderr, "len: %lu\n", len);
        /*if (len > 0 && read_buffer[len - 1] == '\n')
        {
          read_buffer[len - 1] = '\0';
        }*/

        read_buffer[len - 1] = '\0';

        //printf("read_buffer: %s\n", read_buffer);

        char msg[MAX_BUFFER + MAX_USER_SIZE + 2];
        memset(msg, 0, (MAX_BUFFER + MAX_USER_SIZE + 2));

        strncpy(msg, username, strlen(username));
        strcat(msg,": ");
        strcat(msg, read_buffer);

        //printf("message: %s\n", msg);

        //fprintf(stderr,"Read from stdin %zu bytes. sending to server\n", strlen(read_buffer));
        if (send(client_socket, msg, strlen(msg) , 0) != strlen(msg))
        {
          perror("Unable to send message to server\n");
          shut_down(FAILURE);
        }
      }
      else
      {
        fprintf(stderr,"message length: %u > 256 bytes, discarding\n", total_read);
        fflush(STDIN_FILENO);
      }

    }

    if (FD_ISSET(STDIN_FILENO, &except_fds)) 
    {
      fprintf(stderr,"except_fds for stdin.\n");
      shut_down(FAILURE);
    }

    if (FD_ISSET(client_socket, &read_fds)) 
    {
     
      char read_buffer[MAX_BUFFER + MAX_USER_SIZE + 2];
      memset(read_buffer, 0, MAX_BUFFER + MAX_USER_SIZE + 2);
      int valread = recv(client_socket, read_buffer, MAX_BUFFER, 0);

      if (valread == -1)
      {
        perror("unable to read");
        shut_down(FAILURE);
      }

      else if (valread == 0)
      {
        fprintf(stderr,"server closed connection\n");
        shut_down(FAILURE);
      }

      else
      {
        fprintf(fp,"%s\n", read_buffer);
        fflush(fp);
      }

    }

    if (FD_ISSET(client_socket, &except_fds)) 
    {
      fprintf(stderr,"except_fds for server.\n");
      shut_down(FAILURE);
    }
    
  }

}























