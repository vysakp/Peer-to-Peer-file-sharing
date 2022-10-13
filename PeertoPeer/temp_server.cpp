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

#define CHUNK_SIZE 1024
// #define CHUNK_SIZE 512*1024

// using namespace std;

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
    log_string << "Creating a socket with Domain: "<< domain<<"Service:"<<service<<"Protocol:"<<protocol;
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
    /* PUBLIC MEMBER METHODS */
    // The request method allows a client to make a request of a specified server.
    char * (*request)(struct Client *client, char *server_ip, void *request, unsigned long size);
};

char * request_fun(struct Client *client, char *server_ip, void *request, unsigned long size)
{
    // Create an address struct for the server.
    int client_fd;
    struct sockaddr_in server_address;
    // Copy the client's parameters to this address.
    server_address.sin_family = client->domain;
    server_address.sin_port = htons(client->port);
    server_address.sin_addr.s_addr = (int)client->interface;
    // Make the connection.
    inet_pton(client->domain, server_ip, &server_address.sin_addr);
    connect(client->socket, (struct sockaddr*)&server_address, sizeof(server_address));

    // if((client_fd = connect(client->socket, (struct sockaddr*)&server_address, sizeof(server_address)))<0){
    //     std::cout<<"Connection failed";
    //     exit(1);
    // }
    // Send the request;
    send(client->socket, request, size, 0);
    // Read the response.
    char *response = new char[CHUNK_SIZE];
    read(client->socket, response, CHUNK_SIZE);
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

void client_function(char * request, int port){
    std::string temp = std::string(request);
    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, port, INADDR_ANY);
    char server_ip[] = "127.0.0.1";
    int size_of_req = strlen(request);
    client.request(&client, server_ip, request, strlen(request));
}

void write_chunk(int peer_socket, u_long chunk_no, char* file_loc){
    char *buffer = new char[CHUNK_SIZE];
    memset(buffer, 0, CHUNK_SIZE);
    int tot_send = 0;
    while(tot_send<CHUNK_SIZE){
        size_t size = read(peer_socket, buffer, CHUNK_SIZE-1);
        std::fstream outfile(file_loc, std::fstream::in | std::fstream::out | std::fstream::binary);
        outfile.seekp(chunk_no*CHUNK_SIZE+tot_send, std::ios::beg);
        outfile.write(buffer, size);
        outfile.close();

        tot_send+=size;
    }
    

}

void* server_function(void *arg){
    
    int *port = (int*)arg;
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, *port, 20);
    struct sockaddr *address = (struct sockaddr*)&server.address;
    socklen_t address_length = (socklen_t)sizeof(server.address);
    
    
    //Opeing the output file in appending mode
    std::ofstream outfile;
    // outfile.open("./client_2/temp_copy.txt", std::ofstream::binary);
    int d = open("./client_2/test2.jpeg", O_WRONLY | O_CREAT,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (d == -1) {
        // error("Destination file cannot be opened");
        close(d);
        exit(1);
        // return 1;
    }

    // size_t size;
    // while ((size = read(sockfd, buff, BUFSIZ)) > 0) {
    //     write(d, buff, size);
    // }

    while(1){

        int client = accept(server.socket, address, &address_length);

        // char request[255];
        char *buffer = new char[CHUNK_SIZE];
        memset(buffer, 0, CHUNK_SIZE);
        // size_t size;
        // read(client, buffer, CHUNK_SIZE);
        // write(d, buffer, size);

        size_t size;
        while (true) {
            size = read(client, buffer, CHUNK_SIZE);
            if(size<0){
                std::cout<<"Error";
            }
            else if(size==CHUNK_SIZE)
                write(d, buffer, CHUNK_SIZE);
            else{
                write(d, buffer, size);
                break;
                }
        }
        // outfile.write(buffer,20);
        // outfile<<buffer;
        // printf("%s\n", buffer);

        //TODO check when to close the client
        close(client);

    }
    return NULL;
}


u_long get_file_size(){
    std::ifstream in_file("./client_1/test.jpeg", std::ios::binary);
    in_file.seekg(0, std::ios::end);
    u_long file_size = in_file.tellg();
    std::cout<<"Size of the file is"<<" "<< file_size<<" "<<"bytes";
    return file_size;
}



int main(){


    // int c_port=1303, s_port=1403;
    int c_port=1402, s_port=1302;



    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_function, &s_port);




    pthread_join(server_thread,NULL);

    return 0;
}