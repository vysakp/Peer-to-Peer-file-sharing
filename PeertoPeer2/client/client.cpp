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
#include <thread>
#include <chrono>

#include<unordered_map>
#include<unordered_set>
#include<vector>
typedef unsigned long           u_long;



#define TRACKER_PORT 4027
#define WORD_SIZE 1024
#define CHUNK_SIZE 512*1024

struct File{

    std::string file_name;
    std::string file_path;
    int no_of_chunks;
    int size_of_last_chunk;
    std::vector<bool> bit_map;
    File(){

    }
    File(std::string _fname, int _no_chunks){
        file_name = _fname;
        no_of_chunks = _no_chunks;
    }

};

std::unordered_map<std::string, File> ULFile;

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

void connect_to_server(struct Client *client, char* server_ip){
    int client_fd;
    struct sockaddr_in server_address;

    //Tracker's info
    server_address.sin_family = client->domain;
    server_address.sin_port = htons(client->port);
    server_address.sin_addr.s_addr = (int)client->interface;

    inet_pton(client->domain, server_ip, &server_address.sin_addr);

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


std::string BitmapToString(std::string file_name){
    std::vector<bool> bitmap = ULFile[file_name].bit_map;
    std::string res = "";
    for(int i=0;i<bitmap.size();i++){
        if(bitmap[i])
            res+="1";
        else
            res+="0";
    }
    std::string lchunk = std::to_string(ULFile[file_name].size_of_last_chunk);
    res+= (" " + lchunk);
    return res;

}

void get_response(std::string input_cmd, int client_socket, char* response){

    memset(response,0,WORD_SIZE);
    write(client_socket, &input_cmd[0], input_cmd.size());
    //WARNING: we are returning the response from the stack memory of this funciton 
    // char response[WORD_SIZE];
    // memset(response,0,WORD_SIZE);
    size_t size = read(client_socket, response, WORD_SIZE);

    if(size<=0){
        std::cout<<"Read operation failed";
        exit(1);
    }
    // return response;
}

void send_user_message(std::string peer_ip, int peer_port, std::string input_cmd, char* response){
    
    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, peer_port, INADDR_ANY);

    connect_to_server(&client, &peer_ip[0]);

    // char response[WORD_SIZE];
    // memset(response,0,WORD_SIZE);
    // std::cout<<"C side: sending command "<<input_cmd<<'\n';
    get_response(input_cmd, client.socket, response);
    // std::cout<<"C side: got response "<<response<<'\n';

    close(client.socket);
    // std::cout<<response;


}

void ReadChunk(std::string peer_ip, int peer_port, std::string input_cmd, std::string file_loc, int chunk_size){

    //input_cmd: command 
    
    // struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, peer_port, INADDR_ANY);

    // connect_to_server(&client, &peer_ip[0]);

    std::string tmp;
    std::stringstream ss(input_cmd);
    std::vector<std::string> Vinput;
    while(ss>>tmp){
            Vinput.push_back(tmp);
    }
    size_t tot_send = 0;

    int chunk_no = stoi(Vinput[1]);

    // u_long chunk_size = CHUNK_SIZE;
    // std::cout<<"C side: Writing to file "<<file_loc<<'\n';
    int dest = open(&file_loc[0], O_RDWR | O_CREAT, 0666);
    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, peer_port, INADDR_ANY);
    connect_to_server(&client, &peer_ip[0]);


    std::cout<<"C side: Requesting.... "<<'\n';
    write(client.socket, &input_cmd[0], input_cmd.size());

    while(tot_send<=chunk_size){
        char response[chunk_size];
        memset(response,0,chunk_size);
        // std::cout<<"C side: reading buffer "<<'\n';
        // std::cout<<"C side: Reading.... "<<'\n';
        size_t size = read(client.socket, response, CHUNK_SIZE);

        if(size<=0){
            // std::cout<<"Read operation failed";
            break;
        }


        ssize_t nw =  pwrite(dest, response, size, chunk_no*CHUNK_SIZE + tot_send);
        tot_send+=size;
    }
    close(dest);
    close(client.socket);
}

void WriteChunk(int socket_fd, u_long chunk_no, std::string file_name){
    
    std::cout<<"S side: Write chunk "<<'\n';
    // std::ifstream fp(file_loc.c_str(), std::ios::in|std::ios::binary);
    std::ifstream file;
    std::string file_loc = ULFile[file_name].file_path;
    file.open(file_loc, std::ios::binary);
    char *buffer = new char[CHUNK_SIZE];
    memset(buffer, 0, CHUNK_SIZE);

    // file.read(buffer, CHUNK_SIZE);
    // int count = file.gcount();

    file.seekg(chunk_no*CHUNK_SIZE, file.beg);

    

    file.read(buffer, CHUNK_SIZE);
    int count = file.gcount();
    std::cout<<"S side: File opened.. "<<'\n';
    std::cout<<"S side: File size ="<<sizeof(buffer)<<'\n';
    std::cout<<"S side: File size ="<<count<<'\n';
    std::cout<<"S side: File buffer = "<<buffer<<'\n';

    if ((write(socket_fd, buffer, count))< 0) {
        perror("[-]Error in sending file.");
        exit(1);
    }
    std::cout<<"S side: Write to client completed.. "<<'\n';

    file.close();

}

void* handle_connection(int  client_socket){
    // std::cout<<"At handle conneciton\n";
    // while(true){
        // std::cout<<"entered server thread"<<'\n';

        char *client_req = new char[WORD_SIZE];
        memset(client_req, 0, WORD_SIZE);
        size_t size;
        if(read(client_socket, client_req, WORD_SIZE)<=0){
            close(client_socket);
            // break;
        }
        // std::cout<<"Read complete..\n";
        // std::cout<<client_req<<'\n';

        std::string tmp, input = client_req;
        std::stringstream ss(input);
        std::vector<std::string> input_cmd;
        while(ss>>tmp){
                input_cmd.push_back(tmp);
        }
        // std::cout<<"checking commands"<<'\n';
        std::cout<<"S side: got request "<<'\n';
        if(input_cmd[0] == "send_bitmap"){
            // std::cout<<"send_bitmap command matched"<<'\n';
            //send_bitmap file_path
            // std::cout<<input_cmd[0];
            std::cout<<"S side: send_bitmap "<<'\n';
            std::string bit_map = BitmapToString(input_cmd[1]);
            write(client_socket, bit_map.c_str(), bit_map.size());
            // std::cout<<bit_map;

        }
        else if(input_cmd[0] == "send_chunk"){
            std::cout<<"S side: send_chunk "<<'\n';

            //Sending the respective chunk

            WriteChunk(client_socket, stoi(input_cmd[1]), input_cmd[2]);
        }
    //closing the connection after executing the command at server side
    close(client_socket);
    return NULL;

}

void* server_function(void *arg){
    
    int *port = (int*)arg;
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, *port, 20);
    struct sockaddr *address = (struct sockaddr*)&server.address;
    socklen_t address_length = (socklen_t)sizeof(server.address);
    //Vector of threads for each incoming client connection
    std::vector<std::thread> vTreads;
    int client;
    while(true){
        int client_socket;

        // std::cout<<"Entered Accepted ";
        if((client_socket = accept(server.socket, address, &address_length))<0){
            std::cout<<"Accept connection failed..."<<'\n';
        }
        // std::cout<<"Accepted connection";

        vTreads.push_back(std::thread(handle_connection, client_socket));
        // char *buffer = new char[WORD_SIZE];
        // memset(buffer, 0, WORD_SIZE);

        // size_t size;
        // while (true) {
        //     size = read(client, buffer, WORD_SIZE);
        //     if(size<0){
        //         std::cout<<"Error";
        //     }
        //     else
        //         std::cout<<buffer;
        // }
        
    }
    for(auto i=vTreads.begin(); i!=vTreads.end(); i++){
        if(i->joinable()) i->join();
    }
    close(client);
    return NULL;
}



u_long get_file_size(std::string file_path){
    std::ifstream in_file(file_path.c_str(), std::ios::binary);
    in_file.seekg(0, std::ios::end);
    u_long file_size = in_file.tellg();
    // std::cout<<"Size of the file is"<<" "<< file_size<<" "<<"bytes";
    return file_size;
}
int upload_file(std::string file_path){
    if (FILE *file = fopen(file_path.c_str(), "r")) {
        fclose(file);

        std::string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);

        if(ULFile.find(base_filename)!=ULFile.end()){
            //file already added 
            return 0;
        }
        // u_long f_size = get_file_size(file_path);
        // int num_chunks = f_size/(CHUNK_SIZE) + (f_size % (CHUNK_SIZE) != 0);
        // File *new_file = new File(file_path,num_chunks );

        // int l_chunk_size = f_size - (f_size/(CHUNK_SIZE))*CHUNK_SIZE;
        // std::vector<int> b_map(num_chunks, 1);
        // new_file->size_of_last_chunk = l_chunk_size;
        // new_file->bit_map = b_map;

        // ULFile[file_path] = *new_file;
        return 1;

    } 
    else {
        //file doesn't exist
        return -1;
    }
}

int verify_download_file(std::string des_path){
    // if (FILE *file = fopen(src_file.c_str(), "r")) {
    //     fclose(file);

    struct stat info;
    if( stat( des_path.c_str(), &info ) != 0 ){
        //destination path doesn't exist
        return -1;
        // printf( "cannot access %s\n", des_path );
        }
    else if( info.st_mode & S_IFDIR ){  
        //given path exists
        return 1;
        
        }
    else
        {   
            //Given dest_loc is not a directory
            // printf( "%s is no directory\n", des_path );
            return 0;
        }
        
    // }
    // else{
    //     //file doesn't exist
    //     return -2;
    // }
    }

void download_file(std::vector<std::string> users, std::string file_path, std::string src_loc){
    //usr_details[0] = user name 
    //usr_details[1] = user ip
    //usr_details[2] = user port

    //IT is expected the src-loc to be ending with a /

    std::string tmp, cur_usr = users[1];
    std::stringstream ss(cur_usr);
    std::vector<std::string> user_details;
    while(ss>>tmp){
            user_details.push_back(tmp);
    }

    std::string peer_ip = user_details[1];
    int peer_port = stoi(user_details[2]);
    std::string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);

    //First we need to request user for the file details
    // struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, peer_port, INADDR_ANY);

    // connect_to_server(&client, &peer_ip[0]);

    char response[WORD_SIZE];
    memset(response,0,WORD_SIZE);

    std::string input_cmd = "send_bitmap ";
    input_cmd+= file_path;

    send_user_message(peer_ip, peer_port, input_cmd, response);
    

    std::string s_res = response;
    //s_res struct <bit-map> ' ' <size of last chunk>

    // send_user_message(peer_ip, peer_port, input_cmd, response);
    // std::cout<<response;
    std::string bit_map = s_res.substr(0, s_res.find(' '));
    int l_chunk_size = stoi(s_res.substr(s_res.find(" ") + 1)); 
    int i=0;
    //add check if bit map is 1 or not before sending 
    //Continue here...
    // std::cout<<s_res<<' '<<bit_map<<' '<<l_chunk_size<<' '<<bit_map.size()<<'\n';


    for(int i=0;i<bit_map.size();i++){
        input_cmd = "send_chunk ";
        input_cmd += (std::to_string(i) + " ");
        input_cmd += file_path;
        //sending message to user to initiate write operation
        // std::cout<<"C side: sending command "<<input_cmd<<'\n';
        if(i<bit_map.size()-1){
            std::cout<<"C side: Reading chunk.. "<<input_cmd<<'\n';
            ReadChunk(peer_ip, peer_port, input_cmd, src_loc + base_filename, CHUNK_SIZE);
            std::cout<<"C side: Read complete "<<input_cmd<<'\n';
            }
        else{
            std::cout<<"C side: Reading L chunk.. "<<input_cmd<<'\n';
            ReadChunk(peer_ip, peer_port, input_cmd, src_loc + base_filename, l_chunk_size);
            std::cout<<"C side: Read L complete "<<input_cmd<<'\n';
            }
    }

}

std::unordered_map<std::string, std::vector<int>> piecewiseAlgo(std::vector<std::string>users, std::string file_name, std::vector<int> &Lchunk_details ){
    //users[0] ="User List", users[1] = user 1 , users[2] = user 2 ...

    std::string bit_map;
    int l_chunk_size;
    std::unordered_map<std::string, std::string> user_bitmap_map;
    std::unordered_map<std::string, std::vector<int>> user_chunk_map;
    std::unordered_map<int,std::string> user_map;

    for(int i=1;i<users.size();i++){
        std::string tmp, cur_usr = users[i];
        user_map[i-1] = cur_usr;
        std::stringstream ss(cur_usr);
        std::vector<std::string> user_details;
        while(ss>>tmp){
                user_details.push_back(tmp);
        }


        std::string peer_ip = user_details[1];
        int peer_port = stoi(user_details[2]);



        char response[WORD_SIZE];
        memset(response,0,WORD_SIZE);
        std::string input_cmd = "send_bitmap ";
        input_cmd+= file_name;
        send_user_message(peer_ip, peer_port, input_cmd, response);
        std::string s_res = response;
        std::cout<<"M peer: response ="<<response<<'\n';
        // std::cout<<"M peer: here1 "<<'\n';

        bit_map = s_res.substr(0, s_res.find(' '));
        l_chunk_size = stoi(s_res.substr(s_res.find(" ") + 1)); 
        user_bitmap_map[cur_usr] = bit_map;
        // std::cout<<"M peer: here2 "<<'\n';
    }
    // std::cout<<"M peer: here3 "<<'\n';
    Lchunk_details.push_back(bit_map.size());
    Lchunk_details.push_back(l_chunk_size);
    // std::cout<<"M peer: here4 "<<'\n';
    int no_of_users = users.size()-1;
    std::cout<<"M peer: no of users "<<no_of_users<<'\n';

    for(int i=0;i<bit_map.size();i++){
        int to_user = i%no_of_users;
        std::cout<<"M peer: to user = "<<to_user<<'\n';
        if(user_chunk_map.find(user_map[to_user])==user_chunk_map.end()){

            user_chunk_map[user_map[to_user]] = { i };
        }
        else{
            user_chunk_map[user_map[to_user]].push_back(i);
        }
    }

    return user_chunk_map;

}


void single_user_download(std::string user, std::vector<int> chunks, std::vector<int> Lchunk_details ,std::string file_name, std::string src_loc ){
    

    std::cout<<"M peer: at single user"<<'\n';
    std::string input_cmd, tmp;
    std::stringstream ss(user);
    std::vector<std::string> user_details;
    while(ss>>tmp){
            user_details.push_back(tmp);
    }

    std::string peer_ip = user_details[1];
    int peer_port = stoi(user_details[2]);

    // std::string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);

    for(int i=0;i<chunks.size();i++){
        input_cmd = "send_chunk ";
        input_cmd += (std::to_string(chunks[i]) + " ");
        input_cmd += file_name;

        std::cout<<"M peer: reading chunk ="<<chunks[i]<<'\n';
        std::cout<<"M peer: input cmd ="<<input_cmd<<'\n';

        if(chunks[i]==Lchunk_details[0]-1)
            ReadChunk(peer_ip, peer_port, input_cmd, src_loc + file_name, Lchunk_details[1]);
        else
            ReadChunk(peer_ip, peer_port, input_cmd, src_loc + file_name, CHUNK_SIZE);
            
    }
}


void download_file_multi_peer(std::vector<std::string> users, std::string file_name, std::string src_loc){
    std::vector<int> Lchunk_details;
    std::unordered_map<std::string, std::vector<int>> user_chunk_map;
    std::cout<<"M peer: file name = "<<file_name<<'\n';
    // std::string file_path = ULFile[file_name].file_path;
    // std::cout<<"M peer: file path = "<<file_path<<'\n';
    user_chunk_map = piecewiseAlgo(users, file_name, Lchunk_details);
    std::cout<<"M peer: piecewiseAlgo complete"<<'\n';
    std::vector<std::thread> vTreads;
    for(auto user_chunk: user_chunk_map){
        std::string cur_usr = user_chunk.first;
        std::vector<int> chunks = user_chunk.second;
        std::cout<<"M peer: Current user ="<<cur_usr<<'\n';
        // std::cout<<"M peer: Current user ="<<cur_usr<<'\n';
        vTreads.push_back(std::thread(single_user_download, cur_usr, chunks, Lchunk_details, file_name, src_loc));

        
    }
    for(auto i=vTreads.begin(); i!=vTreads.end(); i++){
        if(i->joinable()) i->join();
    }

}



void handleCMD(std::string input_cmd, int client_socket, int server_port){

        std::string tmp, server_ip = "127.0.0.1";
        // write(client_socket, &input_cmd[0], input_cmd.size());

        std::stringstream ss(input_cmd);
        std::vector<std::string> cmd;
        while(ss>>tmp){
                cmd.push_back(tmp);
        }

        // char* response;
        char response[WORD_SIZE];
        memset(response,0,WORD_SIZE);
        // strcpy(response, get_response(input_cmd, client_socket));
        // memset(response,0,WORD_SIZE);
        // size_t size = read(client_socket, response, WORD_SIZE);

        // if(size<=0){
        //     std::cout<<"Read operation failed";
        //     exit(1);
        // }
        // std::cout<<response;
        

        if(cmd[0] == "create_user"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing create_user operation"); 
        }
        else if(cmd[0] == "login"){
            input_cmd += (" " + server_ip);
            input_cmd += (" " + std::to_string(server_port));
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing login operation"); 
        }
        else if(cmd[0] == "create_group"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing create_group operation"); 
        }
        else if(cmd[0] == "join_group"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing join_group operation"); 
        }
        else if(cmd[0] == "leave_group"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing leave_group operation"); 
        }
        else if(cmd[0] == "list_requests"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing list_requests operation"); 
        }
        else if(cmd[0] == "accept_request"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing accept_request operation"); 
        }
        else if(cmd[0] == "list_groups"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing list_groups operation"); 
        }
        else if(cmd[0] == "list_files"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing list_files operation"); 
        }


        else if(cmd[0] == "upload_file"){
            int res = upload_file(cmd[1]);
            if(cmd.size()!=3)
               std::cout<<"Wrong format\nPlease try: upload_file <file_path> <group_id>\n";
            else if(res==-1){
                std::cout<<"Invalid File path\n";
            }
            else if(res==0){
                 std::cout<<"File already uploaded\n";
            }
            else{
                get_response(input_cmd, client_socket, response);
                if(strcmp(response, "File uploaded\n") == 0 ){
                    u_long f_size = get_file_size(cmd[1]);
                    int num_chunks = f_size/(CHUNK_SIZE) + (f_size % (CHUNK_SIZE) != 0);

                    std::string base_filename = cmd[1].substr(cmd[1].find_last_of("/\\") + 1);
                    File *new_file = new File(base_filename, num_chunks );

                    int l_chunk_size = f_size - (f_size/(CHUNK_SIZE))*CHUNK_SIZE;
                    std::vector<bool> b_map(num_chunks, true);
                    new_file->size_of_last_chunk = l_chunk_size;
                    new_file->bit_map = b_map;
                    new_file->file_path = cmd[1];

                    ULFile[base_filename] = *new_file;
                }
                std::cout<<response;
            }
            // get_response(input_cmd, client_socket, response);
            // std::cout<<response;
            // if(strcmp(response, "File uploaded\n") == 0 ){
            //     std::cout<<"Inside the if condition\n";
            //     int res = upload_file(cmd[1]);


            // }
            // else
            //     std::cout<<response;
            c_log.Log(DebugP, "Executing upload_file operation"); 
        }


        else if(cmd[0] == "download_file"){
            int resp = verify_download_file(cmd[3]);
            if(cmd.size()!=4)
               std::cout<<"Wrong format\nPlease try: download_file <group_id> <file_name> <destination_path>n";
            else if(resp == -1){
                std::cout<<"Destination path does not exists\n";
            }
            else if(resp == 0){
                std::cout<<"Destination path is not a directory\n";
            }
            else{
                //TODO: need to add the string cmp for group having the file or not
                //and avoid the -2 check condition
                get_response(input_cmd, client_socket, response);

                // std::cout<<std::string(response);
                std::string resp_prefix = std::string(response).substr(0,9);
                // std::cout<<resp_prefix;
                if(resp_prefix == "User list"){
                    //gives the list of users 
                    std::string s_resp = std::string(response);
                    std::vector<std::string> users;
                    std::string::size_type pos = 0;
                    std::string::size_type prev = 0;
                    while ((pos = s_resp.find('\n', prev)) != std::string::npos) {
                        users.push_back(s_resp.substr(prev, pos - prev));
                        prev = pos + 1;
                    }
                    // users.push_back(s_resp.substr(prev));

                    // for(auto usr: users){
                    //     std::cout<<"M peer: usrs:"<<usr<<'\n';
                    // }

                    // for(auto usr:users)
                    //     std::cout<<usr<<" ";
                    
                    //user 1 = usr[1]
                    // std::string tmp, cur_usr = users[1];
                    // std::stringstream ss(cur_usr);
                    // std::vector<std::string> usr_details;
                    // while(ss>>tmp){
                    //         usr_details.push_back(tmp);
                    // }

                    //usr_details[0] = user name 
                    //usr_details[1] = user ip
                    //usr_details[2] = user port

                    // download_file(users, cmd[2], cmd[3]);
                    std::cout<<"M peer: file name = "<<cmd[2]<<'\n';
                    download_file_multi_peer(users,  cmd[2], cmd[3]);

                    // std::cout<<"hi";

                }
                else
                    std::cout<<response;
            // get_response(input_cmd, client_socket, response);
            // std::cout<<response;
            // c_log.Log(DebugP, "Executing download_file operation"); 
        }
        }


        else if(cmd[0] == "logout"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing logout operation"); 
        }
        else if(cmd[0] == "show_downloads"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing show_downloads operation"); 
        }
        else if(cmd[0] == "stop_share"){
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Executing stop_share operation"); 
        }
        // else if(cmd[0] == "reply_back"){
        //     //for debugging reply_back usr-ip usr-port message
        //     std::cout<<"reply_back executing\n";
        //     send_user_message(cmd[1], std::stoi(cmd[2]),cmd[3]);
        //     std::cout<<"reply_back executed";
        //     // get_response(input_cmd, client_socket, response);
        //     // std::cout<<response;
        //     // c_log.Log(DebugP, "Executing stop_share operation"); 
        // }
        
        else {
            get_response(input_cmd, client_socket, response);
            std::cout<<response;
            c_log.Log(DebugP, "Wrong command executed"); 
        }
        // bzero(response,WORD_SIZE);

}


int main(){
    int port;
    std::cout<<"Enter port number: ";
    std::cin>>port;

    // int port = 2023;

    pthread_t server_thread;

    pthread_create(&server_thread, NULL, server_function, &port);

    // char *cmd = new char[WORD_SIZE];
    // memset(cmd, 0, WORD_SIZE);
    size_t size;

    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, TRACKER_PORT, INADDR_ANY);
    char tracker_ip[] = "127.0.0.1";

    connect_to_server(&client, tracker_ip);


    //ignoring any inputs 
    std::cin.ignore();

    while(true){
        
        std::string tmp, input_cmd;
        std::cout<<"Enter command: ";
        // std::cin>>input_cmd;
        std::getline(std::cin,input_cmd);
        if(input_cmd.size()==0)
            continue;
        // std::cin>>cmd;
        // char* buffer = "Hi";
        // input_cmd = cmd;  

        // write(client.socket, &input_cmd[0], input_cmd.size());


        // char *response = new char[WORD_SIZE];
        // size = read(client.socket, response, WORD_SIZE);

        // if(size<=0){
        //     std::cout<<"Read operation failed";
        //     exit(1);
        // }
        // std::cout<<response;

        // std::stringstream ss(input_cmd);
        // std::vector<std::string> cmd;
        // while(ss>>tmp){
        //         cmd.push_back(tmp);
        // }
        handleCMD(input_cmd, client.socket, port);

        
    }
    pthread_join(server_thread, NULL);
    close(client.socket);

    return 0;
}