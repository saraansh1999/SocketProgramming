#include <stdio.h>  
#include <string.h>
#include <stdlib.h>  
#include <errno.h>  
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>
#include <netdb.h>  
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define COMM_LENGTH 1024
#define BUF_LEN 1000000

void error_handler(char *message)
{
    perror(message);
}

int file_receiver(char *filename, char *buffer, int socket)
{
    int i, in, out;
    
    bzero(buffer, BUF_LEN);
    in = read(socket, buffer, BUF_LEN);
    if(in < 0)
    {
        error_handler("Failure in reading.\n");
        return -1;
    }
    
    if(buffer[0] == '0')
    {
        error_handler("File does not exist.\n");
        return -1;
    }
        
    bzero(buffer, BUF_LEN);
    strcpy(buffer, "Received file existence.\n");
    out = send(socket, buffer, strlen(buffer), 0);         //ack
    if(out != strlen(buffer))
    {
        error_handler("Failure in sending ack.\n");
        return -1;
    }
    
    int new_file = open(filename, O_CREAT | O_RDWR, 0600);
    if(new_file == -1)
    {
        error_handler("Failed to download.\n"); 
        return -1;
    }


    bzero(buffer, BUF_LEN);
    in = read(socket, buffer, BUF_LEN);
    if(in < 0)
    {
        error_handler("Failure in reading.\n");
        return -1;
    }
    write(new_file, buffer, strlen(buffer));
        
    bzero(buffer, BUF_LEN);
    strcpy(buffer, "Received file chunk.\n");
    out = send(socket, buffer, strlen(buffer), 0);         //ack
    if(out != strlen(buffer))
    {
        error_handler("Failure in sending ack.\n");
        return -1;
    }
}

int cli2ser(char* command, int socket)
{
    char buffer[BUF_LEN];
    int out;

    bzero(buffer, BUF_LEN);
    strcpy(buffer, command);
    out = send(socket, buffer, strlen(buffer), 0);
    if(out != strlen(buffer))
    {
        error_handler("Failure in writing.\n");
        return -1;
    }
}

int ser2cli_just_print(char* command, int socket)
{
    char buffer[BUF_LEN];
    int in;

    bzero(buffer, BUF_LEN);
    in = read(socket, buffer, BUF_LEN);
    if(in < 0)
    {
        error_handler("Failure in reading.\n");
        return -1;
    }
    printf("%s", buffer);
}

int main(int argc, char *argv[])
{
    int client_socket, server_port, in, out;
    char buffer[BUF_LEN] = {0};
    struct sockaddr_in server_addr;
    struct hostent *server;
    char command[COMM_LENGTH];

    if(argc < 3)
    {
        error_handler("Provide server name and port number as arguements.\n");
        exit(EXIT_FAILURE);
    }

    server_port = atoi(argv[2]);

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket < 0)
    {
        error_handler("Failure in socket creation.\n");
        exit(EXIT_FAILURE);
    }

    server = gethostbyname(argv[1]);
    if(server == NULL)
    {
        error_handler("Host not found.\n");
        exit(EXIT_FAILURE);
    }

    bzero((char*) &server_addr, sizeof(server_addr));
    bcopy((char *) server->h_addr_list[0], (char *) &server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if(connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
    {
        error_handler("Failed to establish connection.\n");
        exit(EXIT_FAILURE);
    }

    in = read(client_socket, buffer, 1024);
    printf("%s\n", buffer);         //Display welcome message sent by the server

    while(1)
    {
        printf(">> ");
        fgets(command, COMM_LENGTH, stdin);
        command[strlen(command) - 1] = '\0';

        if(strncmp(command, "listall", 7) == 0)
        {
            cli2ser(command, client_socket);

            ser2cli_just_print(command, client_socket);
        }
        else if(strncmp(command, "send", 4) == 0)
        {
            cli2ser(command, client_socket);

            if(file_receiver(command + 5, buffer, client_socket) == -1)
            {
                continue;
            }
            
        }
        else if(strncmp(command, "quit", 4) == 0)
        {
            cli2ser(command, client_socket);

            ser2cli_just_print(command, client_socket);

            printf("Quitting.\n");
            break;
        }
        else
        {
            printf("Invalid command.\n");
        }
    }

    return 0;
}