#include<iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include<pthread.h>
#include <cstring>
#include "./logger.hpp"
#include <sstream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>  

#include<unordered_map>
typedef unsigned long           u_long;



#define TRACKER_PORT 4022
#define WORD_SIZE 1024

BasicLogger c_log;

struct Server{

    int domain;
    int service;
    int protocol;
    int port;
    int socket;
    u_long interface;
    int backlog;
    struct sockaddr_in address;
}; 

struct Server server_constructor(int domain, int service, int protocol, u_long interface, int port,  int backlog){

    std::stringstream log_string;
    log_string << "Creating a server with Domain: "<< domain<<"Service:"<<service<<"Protocol:"<<protocol;
    log_string << "Interface: "<< interface<<"Port: "<<port<<"Backlog:"<<backlog;
    c_log.Log(DebugP, log_string.str());

    struct Server server;
    server.domain = domain;
    server.service = service;
    server.protocol = protocol;
    server.interface = interface;
    server.port = port;
    server.backlog = backlog;

    server.address.sin_family = domain;
    server.address.sin_port = htons(port);

    //to take care of little endian or big endian
    server.address.sin_addr.s_addr = htonl(interface);

    // creating a socket
    log_string.str("");
    log_string << "Creating a server side socket with Domain: "<< domain<<"Service:"<<service<<"Protocol:"<<protocol;
    c_log.Log(DebugP, log_string.str()); 

    server.socket = socket(domain, service, protocol);

    if(server.socket == 0){
        c_log.Log(ErrorP, "Failed to connect to socket");
        exit(1); 
    }

    // Attempt to bind to socket in network
    if ((bind(server.socket, (struct sockaddr *)&server.address, sizeof(server.address))) < 0)
    {   
        c_log.Log(ErrorP, "Failed to bind socket...");
        exit(1);
    }
    // Start listening on the network.
    if ((listen(server.socket, server.backlog)) < 0)
    {
        c_log.Log(ErrorP, "Failed to start listening...");
        exit(1);
    }
    return server;
    
}

void* server_function(void *arg){
    
    int *port = (int*)arg;
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, *port, 20);
    struct sockaddr *address = (struct sockaddr*)&server.address;
    socklen_t address_length = (socklen_t)sizeof(server.address);

    while(true){

        int client = accept(server.socket, address, &address_length);

        char *buffer = new char[WORD_SIZE];
        memset(buffer, 0, WORD_SIZE);

        size_t size;
        while (true) {
            size = read(client, buffer, WORD_SIZE);
            if(size<0){
                std::cout<<"Error";
            }
            else
                std::cout<<buffer;
        }
        close(client);

    }
    return NULL;
}

struct Client
{
    /* PUBLIC MEMBER VARIABLES */
    int socket;
    // Variables dealing with the specifics of a connection.
    int domain;
    int service;
    int protocol;
    int port;
    u_long interface;
};


void connect_to_tracker(struct Client *client, char* tracker_ip){
    int client_fd;
    struct sockaddr_in server_address;

    //Tracker's info
    server_address.sin_family = client->domain;
    server_address.sin_port = htons(client->port);
    server_address.sin_addr.s_addr = (int)client->interface;

    inet_pton(client->domain, tracker_ip, &server_address.sin_addr);

    if((client_fd = connect(client->socket, (struct sockaddr*)&server_address, sizeof(server_address)))<0){
        std::cout<<"Connection failed Server not Live";
        c_log.Log(ErrorP, "Client connection to tracker failed waiting for 1 sec");
        usleep(1000000);
        // exit(1);
    }
}

struct Client client_constructor(int domain, int service, int protocol, int port, u_long interface)
{
    // Instantiate a client object.
    struct Client client;
    client.domain = domain;
    client.port = port;
    client.interface = interface;

    // Establish a socket connection.
    std::stringstream log_string;
    log_string << "Creating a client side socket with Domain: "<< domain<<"Service:"<<service<<"Protocol:"<<protocol;
    c_log.Log(DebugP, log_string.str()); 

    client.socket = socket(domain, service, protocol);
    // Return the constructed socket.
    return client;
}


void handleCMD(char* cmd, int client_socket){

        
        

        

        if(strcmp(cmd, "create_user") == 0){
            c_log.Log(DebugP, "Executing create_user operation"); 
        }
        else if(strcmp(cmd, "login") == 0){
            c_log.Log(DebugP, "Executing login operation"); 
        }
        else if(strcmp(cmd, "create_group") == 0){
            c_log.Log(DebugP, "Executing create_group operation"); 
        }
        else if(strcmp(cmd, "join_group") == 0){
            c_log.Log(DebugP, "Executing join_group operation"); 
        }
        else if(strcmp(cmd, "leave_group") == 0){
            c_log.Log(DebugP, "Executing leave_group operation"); 
        }
        else if(strcmp(cmd, "list_requests") == 0){
            c_log.Log(DebugP, "Executing list_requests operation"); 
        }
        else if(strcmp(cmd, "accept_request") == 0){
            c_log.Log(DebugP, "Executing accept_request operation"); 
        }
        else if(strcmp(cmd, "list_groups") == 0){
            c_log.Log(DebugP, "Executing list_groups operation"); 
        }
        else if(strcmp(cmd, "list_files") == 0){
            c_log.Log(DebugP, "Executing list_files operation"); 
        }
        else if(strcmp(cmd, "upload_file") == 0){
            c_log.Log(DebugP, "Executing upload_file operation"); 
        }
        else if(strcmp(cmd, "download_file") == 0){
            c_log.Log(DebugP, "Executing download_file operation"); 
        }
        else if(strcmp(cmd, "logout") == 0){
            c_log.Log(DebugP, "Executing logout operation"); 
        }
        else if(strcmp(cmd, "show_downloads") == 0){
            c_log.Log(DebugP, "Executing show_downloads operation"); 
        }
        else if(strcmp(cmd, "stop_share") == 0){
            c_log.Log(DebugP, "Executing stop_share operation"); 
        }
        else {
            c_log.Log(DebugP, "Wrong command executed"); 
        }

}


int main(){
    int port;
    std::cout<<"Enter port number: ";
    std::cin>>port;

    // int port = 2023;

    pthread_t server_thread;

    pthread_create(&server_thread, NULL, server_function, &port);

    char *cmd = new char[WORD_SIZE];
    memset(cmd, 0, WORD_SIZE);
    size_t size;

    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, TRACKER_PORT, INADDR_ANY);
    char server_ip[] = "127.0.0.1";

    connect_to_tracker(&client, server_ip);


    //ignoring any inputs 
    std::cin.ignore();

    while(true){
        
        std::string input_cmd;
        std::cout<<"Enter command: ";
        // std::cin>>input_cmd;
        std::getline(std::cin,input_cmd);
        // std::cin>>cmd;
        // char* buffer = "Hi";
        // input_cmd = cmd;  

        write(client.socket, &input_cmd[0], input_cmd.size());


        char *response = new char[WORD_SIZE];
        size = read(client.socket, response, WORD_SIZE);

        if(size<=0){
            std::cout<<"Read operation failed";
            exit(1);
        }
        std::cout<<response;
        // handleCMD(cmd, client.socket)

        
    }
    close(client.socket);

    return 0;
}