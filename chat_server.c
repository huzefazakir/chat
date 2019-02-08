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
#include <unistd.h>
#include <getopt.h>
	
#define TRUE 1 
#define FALSE 0 
#define PORT 9000
#define SERVER_IPV4_ADDR "127.0.0.1" 
#define MAX_CLIENTS 30
#define MAX_BUFFER_SIZE 276
#define MAX_PENDING_CONNECTIONS 10

#define FAILURE -1
#define SUCCESS 0

static int main_socket = - 1;
static int max_sd = -1;

static int client_socket[MAX_CLIENTS];
static char server_ip_address[INET_ADDRSTRLEN];
static uint16_t server_port = 0;

static void shut_down(int code)
{
  printf("Shutdown server\n");
 
  close(main_socket);
  for(uint32_t i = 0; i < MAX_CLIENTS; i++)
  {
    if(client_socket[i] > -1)
      close(client_socket[i]);
  }

  exit(code);
}


void handle_signal_action(int sig_number)
{
  if (sig_number == SIGINT)
  {
    printf("SIGINT was caught!\n");
    shut_down(SUCCESS);
  }

  signal(SIGPIPE, SIG_IGN);
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

  if (sigaction(SIGPIPE, &sa, 0) != 0)
  {
    perror("unable to register handlder for SIGPIPE");
    return FAILURE;
  }

  return SUCCESS;
}


static void init()
{
  for(uint32_t i = 0; i < MAX_CLIENTS; i++)
  {
    client_socket[i] = -1;
  }
}


static int setup_server(struct sockaddr_in *address, int addrlen)
{

  if(address == NULL)
  {
    printf("address is NULL\n");
    return FAILURE;
  }

  int opt = TRUE;
  if( (main_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("failed to create main socket");
    return FAILURE;
  }

  if (setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
  {
    perror("unable to setsockopt");
    close(main_socket);
    return FAILURE;
  }

  address->sin_family = AF_INET;
  //address->sin_addr.s_addr = server_ip_address;
  inet_pton(AF_INET, server_ip_address, &(address->sin_addr));
  address->sin_port = htons(server_port);

  if (bind(main_socket, (struct sockaddr *)address, sizeof(*address)) < 0)
  {
    close(main_socket);
    perror("unable to bind");
    return FAILURE;
  }

  if (listen(main_socket, MAX_PENDING_CONNECTIONS) < 0)
  {
    close(main_socket);
    perror("unable to listen");
    return FAILURE;
  }

  return SUCCESS;
}


static void set_fd_sets(fd_set *read_fds, fd_set *except_fds)
{

  if (read_fds == NULL || except_fds == NULL)
  {
    printf("read_fds or except_fds is NULL\n");
    return;
  }

  //clear the socket set 
  FD_ZERO(read_fds);
  FD_ZERO(except_fds);

  //add main socket to set
  FD_SET(main_socket, read_fds);
  FD_SET(main_socket, except_fds);
  
  //add child sockets to set
  for (uint32_t i = 0; i < MAX_CLIENTS; i++)
  {
    //if valid socket descriptor then add to read list
    if(client_socket[i] > -2)
    {
      FD_SET(client_socket[i], read_fds);
      FD_SET(client_socket[i], except_fds);
    }

    //highest file descriptor number for the select function
    if(client_socket[i] > max_sd)
      max_sd = client_socket[i];
  }
}

static void print_help(char *argv[])
{
  printf("usage: %s --bind-ip <ip address> --bind-port <port no>\n", argv[0]);
  exit(SUCCESS);
}

static void parse_args(int argc, char *argv[])
{
  
  int opt = 0;

  int ip_flag = FALSE;
  int port_flag = FALSE;
  int port = 0;

  while(TRUE)
  {
    int option_index = 0;

    static struct option long_options[] = {
      {"help",           no_argument,         0,  'h'},
      {"bind-ip",        required_argument,   0,  'i'},
      {"bind-port",      required_argument,   0,  'p'},
      {"serve",          no_argument,         0,  's'},
      {0,                0,                   0,   0 }
    };

    opt = getopt_long(argc, argv, "shi:p:", long_options, &option_index);

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

      case 's': break;
       
      default: print_help(argv);

    }
    
  }

  if(ip_flag == FALSE || port_flag == FALSE)
    print_help(argv);

  if(port > 65535)
  {
    printf("Invalid port number: %d\n", port);
    print_help(argv);
  }

  server_port = (uint16_t) port;
 
}

int main(int argc, char *argv[]) 
{
  init();

  if (setup_signals() < 0)
    exit(FAILURE);

  memset(server_ip_address, 0 , sizeof(server_ip_address));
  parse_args(argc, argv);

  printf("server ip address: %s, server_port: %u\n", server_ip_address, server_port);

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));

  int addrlen = sizeof(address);

  if (setup_server(&address, addrlen) < 0)
    exit(FAILURE);

  int new_socket = 0;; 
  int valread = 0;

	char buffer[MAX_BUFFER_SIZE + 1];
  memset(buffer, 0 , sizeof(buffer));
		
	//set of socket descriptors 
	fd_set read_fds;
  fd_set except_fds;
		
	//accept the incoming connection 
	printf("Waiting for connections ...\n"); 
		
	while(TRUE) 
	{

    max_sd = main_socket;
    set_fd_sets(&read_fds, &except_fds);
	
		//wait for an activity on one of the sockets , timeout is NULL , 
		//so wait indefinitely
		int activity = select(max_sd + 1, &read_fds, NULL, &except_fds, NULL);

	  if (activity  == 0 || activity == -1)
    {
      perror("select failure");
      shut_down(FAILURE);
    }

    if (FD_ISSET(main_socket, &except_fds))
    {
      printf("except_fds on main socket");
      shut_down(FAILURE);
    }
			
		//If something happened on the main socket, 
		//then its an incoming connection 
		if (FD_ISSET(main_socket, &read_fds)) 
		{ 
			if ((new_socket = accept(main_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0 ) 
			{ 
				perror("unable to accept incoming connection"); 
				exit(EXIT_FAILURE); 
			} 
			
				
			//add new socket to array of sockets
      uint32_t cl = 0;
			for (cl = 0; cl < MAX_CLIENTS; cl++) 
			{ 
				//if position is empty 
				if(client_socket[cl] == -1) 
				{ 
					client_socket[cl] = new_socket; 
					printf("Adding to list of sockets at index %d, with sd: %d\n" , cl, client_socket[cl]); 
					break; 
				} 
			}

      if (cl == MAX_CLIENTS)
      {
        printf("max client limit reached\n");
        char * msg = "chatroom is full, sorry \r\n";

        if (send(new_socket, msg, strlen(msg), 0) != strlen(msg))
          perror("unable to send msg");

        close(new_socket);
      }

      else
      {
        printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs (address.sin_port));
        char *msg = "Welcome\r\n";

        if(send(new_socket, msg, strlen(msg), 0) != strlen(msg))
          perror("unable to send greeting message");
      }

		} 
			
		//else its some IO operation on some other socket 
		for (uint32_t i = 0; i < MAX_CLIENTS; i++) 
		{
      if (client_socket[i] < 0) continue;

      if (FD_ISSET(client_socket[i], &except_fds))
      {
        printf("client socket exception\n");
        close(client_socket[i]);
        client_socket[i] = -1;
      }
			
      //printf("checking for activity on any client sockets\n");

			if (FD_ISSET(client_socket[i], &read_fds)) 
			{ 
				valread = read(client_socket[i], buffer, MAX_BUFFER_SIZE);

				if (valread == 0) 
				{ 
					//Somebody disconnected , get his details and print 
					getpeername(client_socket[i] , (struct sockaddr*)&address, (socklen_t*)&addrlen); 
					printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port)); 
						
					//Close the socket and mark for reuse 
					close(client_socket[i]); 
					client_socket[i] = -1; 
				}

        else if (valread == -1)
				{
          perror("unable to read");
        }

				//relay back the message that came in 
				else
				{
          int sd = client_socket[i];
					buffer[valread] = '\0';
           
          for(uint32_t k = 0; k < MAX_CLIENTS; k++)
          {
            if (client_socket[k] < 0) continue;

            if (client_socket[k] == sd)
            {
              char msg[MAX_BUFFER_SIZE + 3];
              memset(msg, 0, sizeof(msg));

              int content = 0;
              for(uint32_t idx = 0; idx < strlen(buffer); idx++)
              {
                if(buffer[idx] != ':') 
                {
                  content++;
                  //printf("buffer[%u]: %c\n", idx, buffer[idx]);
                }
                else break;
              }

              strcpy(msg, "me");
              strcat(msg, &buffer[content]);

              if (send(client_socket[k], msg, strlen(msg), 0) != strlen(msg))
              {
                perror("unable to send");
                close(client_socket[k]);
                client_socket[k] = -1;
              }

              continue;

            }

					  if (send(client_socket[k], buffer, strlen(buffer), 0) != strlen(buffer))
            {
              perror("unable to send");
              close(client_socket[k]);
              client_socket[k] = -1;
            }
          }
				} 
			}
		}

	} 
		
	return 0; 
} 

