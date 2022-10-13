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
typedef unsigned long           u_long;

BasicLogger c_log;

struct Server
{
    /* PUBLIC MEMBER VARIABLES */
    int domain;
    int service;
    int protocol;
    u_long interface;
    int port;
    int backlog;
    struct sockaddr_in address;
    int socket;
};

struct Server server_constructor(int domain, int service, int protocol, u_long interface, int port, int backlog)
{
    std::stringstream log_string;
    log_string << "Creating a server with Domain: "<< domain<<"Service:"<<service<<"Protocol:"<<protocol;
    log_string << "Interface: "<< interface<<"Port: "<<port<<"Backlog:"<<backlog;
    c_log.Log(DebugP, log_string.str());

    struct Server server;
    // Define the basic parameters of the server.
    server.domain = domain;
    server.service = service;
    server.protocol = protocol;
    server.interface = interface;
    server.port = port;
    server.backlog = backlog;
    // Use the aforementioned parameters to construct the server's address.
    server.address.sin_family = domain;
    server.address.sin_port = htons(port);
    server.address.sin_addr.s_addr = htonl(interface);
    // Create a socket for the server.
    log_string.str("");
    log_string << "Creating a socket with Domain: "<< domain<<"Service:"<<service<<"Protocol:"<<protocol;
    c_log.Log(DebugP, log_string.str()); 
    server.socket = socket(domain, service, protocol);
    // Initialize the dictionary.
    
    // Confirm the connection was successful.
    if (server.socket == 0)
    {
        c_log.Log(ErrorP, "Failed to connect to socket");
        exit(1);
    }
    // Attempt to bind the socket to the network.
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

struct Client
{
    /* PUBLIC MEMBER VARIABLES */
    // The network socket for handling connections.
    int socket;
    // Variables dealing with the specifics of a connection.
    int domain;
    int service;
    int protocol;
    int port;
    u_long interface;
    /* PUBLIC MEMBER METHODS */
    // The request method allows a client to make a request of a specified server.
    char * (*request)(struct Client *client, char *server_ip, void *request, unsigned long size);
};

char * request_fun(struct Client *client, char *server_ip, void *request, unsigned long size)
{
    // Create an address struct for the server.
    struct sockaddr_in server_address;
    // Copy the client's parameters to this address.
    server_address.sin_family = client->domain;
    server_address.sin_port = htons(client->port);
    server_address.sin_addr.s_addr = (int)client->interface;
    // Make the connection.
    inet_pton(client->domain, server_ip, &server_address.sin_addr);
    connect(client->socket, (struct sockaddr*)&server_address, sizeof(server_address));
    // Send the request;
    send(client->socket, request, size, 0);
    // Read the response.
    char *response = new char[30000];
    read(client->socket, response, 30000);
    return response;
}


struct Client client_constructor(int domain, int service, int protocol, int port, u_long interface)
{
    // Instantiate a client object.
    struct Client client;
    client.domain = domain;
    client.port = port;
    client.interface = interface;
    // Establish a socket connection.
    client.socket = socket(domain, service, protocol);
    client.request = request_fun;
    // Return the constructed socket.
    return client;
}

void client_function(char * request){
    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, 1255, INADDR_ANY);
    char server_ip[] = "127.0.0.1";
    client.request(&client, server_ip, request, sizeof(request));
}

void* server_function(void *arg){
    
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, 1255, 20);
    struct sockaddr *address = (struct sockaddr*)&server.address;
    socklen_t address_length = (socklen_t)sizeof(server.address);
    
    while(1){

        int client = accept(server.socket, address, &address_length);

        char request[255];
        memset(request, 0, 255);
        read(client, request, 255);
        printf("%s/n", request);
        close(client);

    }
    return NULL;
}


int main(){

    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_function, NULL);

    while(1){
        char request[255];
        memset(request,0,255);
        std::cin>>request;
        // char request[] = "helloo";
        // std::cout<<"main"<<'\n';
        client_function(request);  
    }
    pthread_join(server_thread,NULL);

    return 0;
}