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
#include<vector>
#include <chrono>
#include <thread>

typedef unsigned long           u_long;

BasicLogger TrLog;

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
    TrLog.Log(DebugP, log_string.str());

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
    TrLog.Log(DebugP, log_string.str()); 

    server.socket = socket(domain, service, protocol);

    if(server.socket == 0){
        TrLog.Log(ErrorP, "Failed to connect to socket");
        exit(1); 
    }

    // Attempt to bind to socket in network
    if ((bind(server.socket, (struct sockaddr *)&server.address, sizeof(server.address))) < 0)
    {   
        TrLog.Log(ErrorP, "Failed to bind socket...");
        exit(1);
    }
    // Start listening on the network.
    if ((listen(server.socket, server.backlog)) < 0)
    {
        TrLog.Log(ErrorP, "Failed to start listening...");
        exit(1);
    }
    return server;
    
}

void* handle_connection(int  client_socket){

    // int* client_socket = (int *)arg;

    while(true)
    {
        char client_req[1024] = {0};
        size_t size;
        size = read(client_socket, client_req, 1024);

        if(size<=0){
            close(client_socket);
            break;
        }
        std::string input = client_req;

        if(input == "create_user"){
            write(client_socket, "Input is create_user", 20);
        }
        else if(input == "login"){
            write(client_socket, "Input is login", 20);
        }
        else if(input == "create_group"){
            write(client_socket, "Input is create_group", 20);
        }
        else if(input == "join_group"){
            write(client_socket, "Input is join_group", 20);
        }
        else if(input == "leave_group"){
            write(client_socket, "Input is leave_group", 20);
        }
        else if(input == "list_requests"){
            write(client_socket, "Input is list_requests", 20);
        }
        else if(input == "accept_request"){
            write(client_socket, "Input is accept_request", 20);
        }
        else if(input == "list_groups"){
            write(client_socket, "Input is list_groups", 20);
        }
        else if(input == "list_files"){
            write(client_socket, "Input is list_files", 20);
        }
        else if(input == "upload_file"){
            write(client_socket, "Input is upload_file", 20);
        }
        else if(input == "download_file"){
            write(client_socket, "Input is download_file", 20);
        }
        else if(input == "logout"){
            write(client_socket, "Input is logout", 20);
        }
        else if(input == "show_downloads"){
            write(client_socket, "Input is show_downloads", 20);
        }
        else if(input == "stop_share"){
            write(client_socket, "Input is stop_share", 20);
        }
        else {
            write(client_socket, "Invalid command", 20);
        }


    }
    close(client_socket);

    return NULL;


}

void* quit_function(void* arg){
    while(true){
        std::string input;
        std::getline(std::cin, input);
        if(input=="quit"){
            std::cout<<"closing the Tracker";
            int *tracker_fd = (int*) arg;
            close(*tracker_fd);
            exit(1);
        }

    }
}

int main(){

    int port = 4000;
    struct Server Tracker = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, port, 20);
    struct sockaddr *address = (struct sockaddr*)&Tracker.address;
    socklen_t address_length = (socklen_t)sizeof(Tracker.address);
    pthread_t exitTread;

    pthread_create(&exitTread, NULL, quit_function, &Tracker.socket);

    //Vector of threads for each incoming client connection
    std::vector<std::thread> vTreads;

    while(true){
        int client_socket;
        
        if((client_socket = accept(Tracker.socket, address, &address_length))<0){
            std::cout<<"connection created...";
            perror("Tracker failed to accept");
        }

        //creating a new thread for the client 
        // pthread_t new_client;
        // pthread_create(&new_client, NULL, handle_connection, (void *)client_socket);
        vTreads.push_back(std::thread(handle_connection, client_socket));
        // handle_connection(client_socket);
    }

    // for(pthread_t client: vTreads){
    //     pthread_join(client, NULL);
    // }

    return 0;
}
