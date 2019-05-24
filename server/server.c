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
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_CLIENTS 50
#define BUF_LEN 1000000

char* itoa(int val, int base){
	
	static char buf[32] = {0};
	
	int i = 30;
	
	for(; val && i ; --i, val /= base)
	
		buf[i] = "0123456789abcdef"[val % base];
	
	return &buf[i+1];
	
}

void error_handler(char *message)
{
    perror(message);
}

void listall(char *buffer)
{
    DIR *dir;
    struct dirent *entry;
    dir = opendir("./");
    if(dir == NULL)
    {
        strcpy(buffer, "Server failed to read its contents.\n");
        return;
    }
    while((entry = readdir(dir)) != NULL)
    {
        if(entry->d_name[0] != '.')
        {
            strcat(buffer, entry->d_name);
            strcat(buffer, "\n");
        }
    }
    closedir(dir);
}

int file_sender(char *buffer, int d, char *ip, int port)
{
    int in, out;
    char filename[BUF_LEN];
    strcpy(filename, buffer + 5);

    struct stat file_info;
    
    int file = open(filename, O_RDONLY);

    bzero(buffer, BUF_LEN);
    if(file == -1)
    {
        strcpy(buffer, "0");
        
        error_handler("Server can't open file.\n");
        out = send(d, buffer, strlen(buffer), 0);
        return -1;
    }
    else
    {
        strcpy(buffer, "1");
        out = send(d, buffer, strlen(buffer), 0);
    }
    if(out < strlen(buffer))
    {
        error_handler("Failure in acknowledging the existence of the file to the client.\n");
        return -1;
    }
    
    bzero(buffer, BUF_LEN);
    in = read(d, buffer, BUF_LEN);   //ack
    if(in < 0)
    {
        error_handler("Failed to recieve ack.\n");
        return -1;
    }
    
    
    stat(filename, &file_info);
    
    bzero(buffer, BUF_LEN);
    read(file, buffer, file_info.st_size);

    out = send(d, buffer, strlen(buffer), 0);
    if(out < strlen(buffer))
    {
        error_handler("Failure in sending the file.\n");
        return -1;
    }

    bzero(buffer, BUF_LEN);
    in = read(d, buffer, BUF_LEN);   //ack
    if(in < 0)
    {
        error_handler("Failed to recieve ack.\n"); 
        return -1;
    }

    printf("File(%s) sent to Client(IP: %s  Port: %d) and acknowledged.\n", filename, ip, port);
}

int main(int argc, char *argv[])
{
    int server_socket, new_descriptor, server_port, address_length, in, out, pid;
    int opt = 1;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUF_LEN];

    if(argc < 2)
    {
        error_handler("Port for server not provided.\n");
        exit(EXIT_FAILURE);
    }

    server_port = atoi(argv[1]);
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1)
    {
        error_handler("Failure in Server socket creation.\n");
        exit(EXIT_FAILURE);
    }

    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        error_handler("Failure in setsockopt.\n");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
    {
        error_handler("Failure in binding server.\n");
        exit(EXIT_FAILURE);
    }

    if(listen(server_socket, 5) < 0)
    {
        error_handler("Failure in listening.\n");
        exit(EXIT_FAILURE);
    }

    address_length = sizeof(client_addr);

    while(1)
    {   
        new_descriptor = accept(server_socket, (struct sockaddr *) &client_addr, &address_length);
        if(new_descriptor < 0)
        {
            error_handler("Failure in acceepting client request.\n");
            continue;
        }
       
        pid = fork();
        if(pid < 0)
        {
            error_handler("Failure in threading.\n");
            continue;
        }

        if(pid == 0)
        {
            break;           
        }
        else
        {
            close(new_descriptor);
        }
        
    }

    if(pid == 0)
    {
        close(server_socket);

        printf("New Client connected:\nSocket: %d\nIP: %s\nPort: %d\n", new_descriptor, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        bzero(buffer, BUF_LEN);
        strcpy(buffer, "WELCOME!");
        out = send(new_descriptor, buffer, strlen(buffer), 0);
        if(out != strlen(buffer))
        {
            error_handler("Failure in sending welcome message.");
            exit(EXIT_FAILURE);
        }
        
        while(1)
        {   
            bzero(buffer, BUF_LEN);
            in = read(new_descriptor, buffer, BUF_LEN);
            if(in < 0)
            {
                error_handler("Failure in reading client's command.\n");
                continue;
            }
            
            if(strncmp(buffer, "listall", 7) == 0)
            {
                printf("Client(IP: %s  Port: %d) requests 'listall'\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                
                bzero(buffer, BUF_LEN);
                listall(buffer);
                send(new_descriptor, buffer, strlen(buffer), 0);
            }
            else if(strncmp(buffer, "send", 4) == 0)
            {
                printf("Client(IP: %s  Port: %d) requests 'send' for %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer + 5);
                
                if(file_sender(buffer, new_descriptor, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)) == -1)
                    continue;
            }
            else if(strncmp(buffer, "quit", 4) == 0)
            {
                printf("Client(IP: %s  Port: %d) disconnecting\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                bzero(buffer, BUF_LEN);
                strcpy(buffer, "Disconnecting.\n");
                send(new_descriptor, buffer, strlen(buffer), 0);

                return 0;
            }
        }
    }
    
    return 0;
}