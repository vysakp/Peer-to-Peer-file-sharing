#include<iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include<pthread.h>
#include <cstring>
#include "../logger.hpp"
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

#define TRACKER_PORT 4030
#define WORD_SIZE 10*1024
#define CHUNK_SIZE 512*1024




BasicLogger c_log;
const char* BasicLogger::filepath = "client_log.txt";



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

struct Client
{
    int socket;
    int domain;
    int service;
    int protocol;
    int port;
    u_long interface;
};

Client curTracker;
std::string tracker1_ip;
std::string tracker2_ip;
int tracker1_port;
int tracker2_port;
bool tracker1_status = true;
bool tracker2_status = true;
bool switched = false;

//map of all files
//filename->File
std::unordered_map<std::string, File> ULFile;

bool isLoggedIn = false;
std::string curUser = "";


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

    //To take care of little endian or big endian
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


void connect_to_server(struct Client *client, char* server_ip){
    /* funciton to connect the given client to server ip
    */

    int client_fd;
    struct sockaddr_in server_address;

    //Server's info
    server_address.sin_family = client->domain;
    server_address.sin_port = htons(client->port);
    server_address.sin_addr.s_addr = (int)client->interface;

    inet_pton(client->domain, server_ip, &server_address.sin_addr);

    if((client_fd = connect(client->socket, (struct sockaddr*)&server_address, sizeof(server_address)))<0){
        // std::cout<<"Connection failed server not Live"<<'\n';
        c_log.Log(ErrorP, "Client connection to tracker failed waiting for 1 sec");
        if(strcmp(server_ip,tracker2_ip.c_str())==0){
            // std::cout<<"Connection failed tracker 2 not Live"<<'\n';
            tracker2_status = false;
            }
        else if(strcmp(server_ip,tracker1_ip.c_str())==0){
            std::cout<<"Connection failed tracker 1 not Live"<<'\n';
            tracker1_status = false;
            }
        else
            std::cout<<"Connection failed server not Live"<<'\n';
        
        // usleep(1000000);
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
    /* Function that returns the bitmap of the given file in string format
    */
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

std::string execute_cmd(std::string command) {
   /*Funtion that execute a command on system and return the result
   */

   char buffer[128];
   std::string result = "";

   // Open pipe to file
   FILE* pipe = popen(command.c_str(), "r");
   if (!pipe) {
      return "popen failed!";
   }

   // read till end of process:
   while (!feof(pipe)) {
      // use buffer to read and add to result
      if (fgets(buffer, 128, pipe) != NULL)
         result += buffer;
   }
   pclose(pipe);

   return result;

}

void switch_tracker(){

    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, tracker2_port, INADDR_ANY);
    curTracker = client;
    connect_to_server(&client, &tracker2_ip[0]);
    switched = true;
}

void send_to_tracker2(std::string input_cmd){

    char dummy_resp[WORD_SIZE];
    memset(dummy_resp,0,WORD_SIZE);
    struct Client tracker2 = client_constructor(AF_INET, SOCK_STREAM, 0, tracker2_port, INADDR_ANY);
    connect_to_server(&tracker2, &tracker2_ip[0]);
    
    if(tracker2_status){
        write(tracker2.socket, &input_cmd[0], input_cmd.size());
        size_t size2 = read(tracker2.socket, dummy_resp, WORD_SIZE);
    }
    close(tracker2.socket);

}

void get_response(std::string input_cmd, int client_socket, char* response){
    /*Funciton that sends the input command ot the client socket and stores the
     response in respose argument */
    
    memset(response,0,WORD_SIZE);

    write(client_socket, &input_cmd[0], input_cmd.size());
    size_t size = read(client_socket, response, WORD_SIZE);
    
    // if(!switched)
    //     send_to_tracker2(input_cmd);

    if(size<=0){
        std::cout<<"Read operation failed switching tracker";
        // exit(1);
        switch_tracker();
    }
}

void send_user_message(std::string peer_ip, int peer_port, std::string input_cmd, char* response){
    /*Funciton to send a input_cmd message to given peer and ip nad stores the response in response
    argument */
    
    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, peer_port, INADDR_ANY);

    connect_to_server(&client, &peer_ip[0]);

    get_response(input_cmd, client.socket, response);

    close(client.socket);

}

void ReadChunk(std::string peer_ip, int peer_port, std::string input_cmd, std::string file_loc, int chunk_size){
    /*Funciton to read and chunk of data. The fuction will request for the chunk and then receive the response,
    and write the respose to the given file location*/

    std::string tmp;
    std::stringstream ss(input_cmd);
    std::vector<std::string> Vinput;
    while(ss>>tmp){
            Vinput.push_back(tmp);
    }
    size_t tot_send = 0;
    int chunk_no = stoi(Vinput[1]);

    int dest = open(&file_loc[0], O_RDWR | O_CREAT, 0666);
    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, peer_port, INADDR_ANY);
    connect_to_server(&client, &peer_ip[0]);

    //Requesting client server to write the chunk
    write(client.socket, &input_cmd[0], input_cmd.size());
    //Adding a loop to recieve the entire buffer
    while(tot_send<=chunk_size){

        char response[chunk_size];
        memset(response,0,chunk_size);
        size_t size = read(client.socket, response, CHUNK_SIZE);

        if(size<=0){
            // Read operation complete or failed
            break;
        }
        ssize_t nw =  pwrite(dest, response, size, chunk_no*CHUNK_SIZE + tot_send);
        tot_send+=size;

    }
    //Setting the bitmap value of chunk no to true
    std::string base_filename = file_loc.substr(file_loc.find_last_of("/\\") + 1);
    ULFile[base_filename].bit_map[chunk_no] = true;
    close(dest);
    close(client.socket);
}

void WriteChunk(int socket_fd, u_long chunk_no, std::string file_name){
    /*Funciton to read the chunk data from the given file location and write the data onto the 
    given socket file descriptor*/
    
    std::ifstream file;
    std::string file_loc = ULFile[file_name].file_path;
    file.open(file_loc, std::ios::binary);
    char *buffer = new char[CHUNK_SIZE];
    memset(buffer, 0, CHUNK_SIZE);

    file.seekg(chunk_no*CHUNK_SIZE, file.beg);

    file.read(buffer, CHUNK_SIZE);
    int count = file.gcount();

    if ((write(socket_fd, buffer, count))< 0) {
        perror("[-]Error in sending file.");
        exit(1);
    }
    file.close();

}

void* handle_connection(int  client_socket){
    /*Funciton to handle requests to  server side*/

    char *client_req = new char[WORD_SIZE];
    memset(client_req, 0, WORD_SIZE);
    size_t size;
    if(read(client_socket, client_req, WORD_SIZE)<=0){
        close(client_socket);
    }

    std::string tmp, input = client_req;
    std::stringstream ss(input);
    std::vector<std::string> input_cmd;
    while(ss>>tmp){
            input_cmd.push_back(tmp);
    }

    if(input_cmd[0] == "send_bitmap"){
        std::string bit_map = BitmapToString(input_cmd[1]);
        write(client_socket, bit_map.c_str(), bit_map.size());
    }
    else if(input_cmd[0] == "send_chunk"){
        WriteChunk(client_socket, stoi(input_cmd[1]), input_cmd[2]);
    }
    
    close(client_socket);
    return NULL;

}

void* server_function(void *arg){
     /*Server funciton to handle download requests from client side,
     multi threaded to handle multiple requests*/
    
    int *port = (int*)arg;
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, *port, 20);
    struct sockaddr *address = (struct sockaddr*)&server.address;
    socklen_t address_length = (socklen_t)sizeof(server.address);

    //Vector of threads for each incoming client connection
    std::vector<std::thread> vTreads;
    int client;

    while(true){
        int client_socket;

        if((client_socket = accept(server.socket, address, &address_length))<0){
            std::cout<<"Accept connection failed..."<<'\n';
        }

        vTreads.push_back(std::thread(handle_connection, client_socket));
        
    }

    for(auto i=vTreads.begin(); i!=vTreads.end(); i++){
        if(i->joinable()) i->join();
    }
    close(client);
    return NULL;

}

u_long get_file_size(std::string file_path){
    /* Helper funciton which return the file size of the the given file
    */
    std::ifstream in_file(file_path.c_str(), std::ios::binary);
    in_file.seekg(0, std::ios::end);
    u_long file_size = in_file.tellg();
    return file_size;
}

int verify_upload_file(std::string file_path){
    /* Helper funciton verify the paths before uploading the file metadata
    */

    if (FILE *file = fopen(file_path.c_str(), "r")) {
        fclose(file);
        std::string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);

        if(ULFile.find(base_filename)!=ULFile.end()){
            //file already added 
            return 0;
        }

        return 1;
    } 

    else {
        //file doesn't exist
        return -1;
    }
}

int verify_download_file(std::string des_path){
    /* Helper funciton verify the paths before downloading the file
    */

    struct stat info;
    if( stat( des_path.c_str(), &info ) != 0 ){
        //destination path doesn't exist
        return -1;
        }
    else if( info.st_mode & S_IFDIR ){  
        //given path exists
        return 1;
        }
    else{   
        //Given dest_loc is not a directory
        return 0;
        }

    }

std::string get_chunk_sha(u_long chunk_no, std::string file_loc){
    /* Funciton to get the sha1 value of the given file chunk
    */

    std::ifstream file;
    file.open(file_loc, std::ios::binary);
    char *buffer = new char[CHUNK_SIZE];
    memset(buffer, 0, CHUNK_SIZE);
    file.seekg(chunk_no*CHUNK_SIZE, file.beg);
    file.read(buffer, CHUNK_SIZE);
    file.close();
    
    //creating a random number for temp file creation
    //calculating the sha value on this temp file as openssl lib
    //is not working for mac
    srand(time(0));
    std::string temp_chunk_file_name = "temp" + std::to_string(rand())+ ".txt";
    std::ofstream out(temp_chunk_file_name);
    out<<buffer;
    out.close();

    std::string cmd = "sha1sum " + temp_chunk_file_name;
    std::string chunk_sha = execute_cmd(cmd);
    remove(temp_chunk_file_name.c_str());

    chunk_sha = chunk_sha.substr(0, chunk_sha.find(' '));
    return chunk_sha;

}

void upload_file(std::string file_path, int tracker_socket){
    /* Funciton to upload a file and update the necessary data at tracker
    */

    char response[WORD_SIZE];
    memset(response,0,WORD_SIZE);
    u_long f_size = get_file_size(file_path);
    int num_chunks = f_size/(CHUNK_SIZE) + (f_size % (CHUNK_SIZE) != 0);

    std::string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
    File *new_file = new File(base_filename, num_chunks );

    int l_chunk_size = f_size - (f_size/(CHUNK_SIZE))*CHUNK_SIZE;
    std::vector<bool> b_map(num_chunks, true);
    new_file->size_of_last_chunk = l_chunk_size;
    new_file->bit_map = b_map;
    new_file->file_path = file_path;

    ULFile[base_filename] = *new_file;

    // Sending the file details update command to tracker
    //file_update <file_name> <file_size> <last_chunk_size>
    std::string file_update_cmd = "file_update " + base_filename + " ";
    file_update_cmd += (std::to_string(num_chunks)+ " " + std::to_string(l_chunk_size));

    memset(response,0,WORD_SIZE);
    get_response(file_update_cmd, tracker_socket, response);

    //Sending the sha value to tracker side
    //chunk_sha_update <file_name> <chunk_no> <sha1sum>
    for(int i=0;i<num_chunks; i++){
        std::string chunk_sha = get_chunk_sha(i, file_path);
        memset(response,0,WORD_SIZE);
        std::string chunk_sha_update_cmd = "chunk_sha_update " + base_filename + " ";
        chunk_sha_update_cmd += (std::to_string(i)+ " " + chunk_sha);
        get_response(chunk_sha_update_cmd, tracker_socket, response);
    }

}

void download_file(std::vector<std::string> users, std::string file_path, std::string src_loc){
    /* Temp Funciton to download a file from a single user, used for debugging
    */

    std::string tmp, cur_usr = users[1];
    std::stringstream ss(cur_usr);
    std::vector<std::string> user_details;
    while(ss>>tmp){
            user_details.push_back(tmp);
    }

    std::string peer_ip = user_details[1];
    int peer_port = stoi(user_details[2]);
    std::string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);

    char response[WORD_SIZE];
    memset(response,0,WORD_SIZE);

    std::string input_cmd = "send_bitmap ";
    input_cmd+= file_path;

    send_user_message(peer_ip, peer_port, input_cmd, response);
    

    std::string s_res = response;

    //s_res struct <bit-map>  <size of last chunk>
    std::string bit_map = s_res.substr(0, s_res.find(' '));
    int l_chunk_size = stoi(s_res.substr(s_res.find(" ") + 1)); 
    int i=0;

    for(int i=0;i<bit_map.size();i++){
        input_cmd = "send_chunk ";
        input_cmd += (std::to_string(i) + " ");
        input_cmd += file_path;

        //sending message to user to initiate write operation
        if(i<bit_map.size()-1)
            ReadChunk(peer_ip, peer_port, input_cmd, src_loc + base_filename, CHUNK_SIZE);
        else
            ReadChunk(peer_ip, peer_port, input_cmd, src_loc + base_filename, l_chunk_size);
    }

}

std::unordered_map<std::string, std::vector<int>> piecewiseAlgo(std::vector<std::string>users, std::string file_name, std::vector<int> &Lchunk_details ){
    /* PieceWiseAlgorithm to decide on the downloading order from different users. 
    Updates the Lchunk_details argument with the lastchunk details and returns the 
    user chunk-list map
    */

    std::string bit_map;
    int l_chunk_size;
    std::unordered_map<std::string, std::string> user_bitmap_map;
    std::unordered_map<std::string, std::vector<int>> user_chunk_map;
    std::unordered_map<int,std::string> user_map;

    //For loop to get the bit map details of the users
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

        std::cout<<s_res.substr(s_res.find(" ") + 1);
        bit_map = s_res.substr(0, s_res.find(' '));
        l_chunk_size = stoi(s_res.substr(s_res.find(" ") + 1)); 
        user_bitmap_map[cur_usr] = bit_map;
    }


    Lchunk_details.push_back(bit_map.size());
    Lchunk_details.push_back(l_chunk_size);
    int no_of_users = users.size()-1;

    //For loop the assign the chunk to different users
    for(int i=0;i<bit_map.size();i++){
        int to_user = i%no_of_users;
        if(user_chunk_map.find(user_map[to_user])==user_chunk_map.end())
            user_chunk_map[user_map[to_user]] = { i };
        else
            user_chunk_map[user_map[to_user]].push_back(i);
    }

    return user_chunk_map;

}

void verify_chunk_sha(std::string down_file_loc, std::string file_name,  int chunk_no, int tracker_socket){
    /* Funtion to verify the download file chunk sha and the sha at tracker side
    */

    char response[WORD_SIZE];
    memset(response,0,WORD_SIZE);
    std::string down_chunk_sha = get_chunk_sha(chunk_no, down_file_loc);

    //verify_sha <file_name> <chunk_no> <chunk_sha> 
    std::string verify_cmd = "verify_sha " + file_name + " ";
    verify_cmd += (std::to_string(chunk_no) + " " + down_chunk_sha);
    get_response(verify_cmd, tracker_socket, response);

}

void single_user_download(std::string user, std::vector<int> chunks, std::vector<int> Lchunk_details ,std::string file_name, std::string src_loc, int tracker_socket){
    /* Function which downloads the gives vector of chunks from the given user and write the
    buffer onto the given file
    */
    std::string input_cmd, tmp;
    std::stringstream ss(user);
    std::vector<std::string> user_details;
    while(ss>>tmp){
            user_details.push_back(tmp);
    }
    std::string peer_ip = user_details[1];
    int peer_port = stoi(user_details[2]);

    //Reading the chunks from from the server and writing it in the file
    for(int i=0;i<chunks.size();i++){
        //sleeping fow .1s to  reduce the download speed
        // usleep(100000);
        
        input_cmd = "send_chunk ";
        input_cmd += (std::to_string(chunks[i]) + " ");
        input_cmd += file_name;
        if(chunks[i]==Lchunk_details[0]-1)
            ReadChunk(peer_ip, peer_port, input_cmd, src_loc + file_name, Lchunk_details[1]);
        else
            ReadChunk(peer_ip, peer_port, input_cmd, src_loc + file_name, CHUNK_SIZE);
        
        // verify_chunk_sha(src_loc + file_name, file_name , chunks[i], tracker_socket);
    }

}

void update_file_data(int tracker_socket, std::vector<int> Lchunk_details, std::string file_path, std::string group_name){
    /* Function to create a file data at user and tracker side and set the bitmap as false
    */

    std::string upload_cmd = "upload_file " + file_path + " " + group_name + " " + curUser;
    int no_chunks = Lchunk_details[0];
    int l_chunk_size = Lchunk_details[1];

    char response[WORD_SIZE];
    memset(response,0,WORD_SIZE);
    get_response(upload_cmd, tracker_socket, response);

    if(strcmp(response, "File uploaded\n") == 0 ){
        std::string base_filename = file_path.substr(file_path.find_last_of("/\\") + 1);
        File *new_file = new File(base_filename, no_chunks );
        std::vector<bool> b_map(no_chunks, false);
        new_file->size_of_last_chunk = l_chunk_size;
        new_file->bit_map = b_map;
        new_file->file_path = file_path;
        ULFile[base_filename] = *new_file;
    }

}

void download_file_multi_peer(std::vector<std::string> users, std::string file_name, std::string src_loc, std::string group_name, int tracker_socket){
    /* Function to download the given file from multiple peers and write it onto
       the given file location
    */
    
    char response[WORD_SIZE];
    memset(response,0,WORD_SIZE);

    //Setting the download status = D
    //down_status_update <file_name> <group_id> <status>
    std::string status_cmd = "down_status_update " + file_name + " [D][" + group_name + "]";
    get_response(status_cmd, tracker_socket, response);


    std::vector<int> Lchunk_details;
    std::unordered_map<std::string, std::vector<int>> user_chunk_map;
    user_chunk_map = piecewiseAlgo(users, file_name, Lchunk_details);
    update_file_data(tracker_socket, Lchunk_details, src_loc+file_name,group_name);

    std::vector<std::thread> vTreads;
    for(auto user_chunk: user_chunk_map){
        std::string cur_usr = user_chunk.first;
        std::vector<int> chunks = user_chunk.second;
        vTreads.push_back(std::thread(single_user_download, cur_usr, chunks, Lchunk_details, file_name, src_loc, tracker_socket));
    }
    for(auto i=vTreads.begin(); i!=vTreads.end(); i++){
        if(i->joinable()) i->join();
    }
    //Setting the download status = C
    //down_status_update <file_name> <group_id> <status>
    status_cmd = "down_status_update " + file_name + " [C][" + group_name + "]";
    get_response(status_cmd, tracker_socket, response);
}

void handleCMD(std::string input_cmd, int client_socket, int server_port, std::vector<std::thread> &DUThreads){
    /* Function to handle the user commands and request the same with tracker and peer server
    */

    std::string tmp, server_ip = "127.0.0.1";

    std::stringstream ss(input_cmd);
    std::vector<std::string> cmd;
    while(ss>>tmp){
            cmd.push_back(tmp);
    }

    char response[WORD_SIZE];
    memset(response,0,WORD_SIZE);
    
    if(cmd[0] == "create_user"){
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing create_user operation"); 
    }

    else if(cmd[0] == "login"){
        if(isLoggedIn){
            std::cout<<"Please logout to login\n";
            return;
        }
        input_cmd += (" " + server_ip);
        input_cmd += (" " + std::to_string(server_port));
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        if(strcmp(response,"Login successful\n") == 0){
            isLoggedIn = true;
            curUser = cmd[1];
            }
        c_log.Log(DebugP, "Executing login operation"); 
    }

    else if(cmd[0] == "create_group"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login to create group\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing create_group operation"); 
    }

    else if(cmd[0] == "join_group"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login to join group\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing join_group operation"); 
    }

    else if(cmd[0] == "leave_group"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login to leave group\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing leave_group operation"); 
    }

    else if(cmd[0] == "list_requests"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing list_requests operation"); 
    }

    else if(cmd[0] == "accept_request"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing accept_request operation"); 
    }

    else if(cmd[0] == "list_groups"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing list_groups operation"); 
    }

    else if(cmd[0] == "list_files"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing list_files operation"); 
    }

    else if(cmd[0] == "upload_file"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login\n";
            return;
        }
        int res = verify_upload_file(cmd[1]);
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
                //Adding the function to download upload threads
                DUThreads.push_back(std::thread(upload_file, cmd[1], client_socket));
            }
            std::cout<<response;
        }
        c_log.Log(DebugP, "Executing upload_file operation"); 

    }


    else if(cmd[0] == "download_file"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"Please login\n";
            return;
        }
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
            get_response(input_cmd, client_socket, response);

            std::string resp_prefix = std::string(response).substr(0,9);
            // cheking the prefix
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

                DUThreads.push_back(std::thread(download_file_multi_peer,  users, cmd[2], cmd[3], cmd[1], client_socket));
            }

            else
                std::cout<<response;
        }

    }

    else if(cmd[0] == "logout"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"No users logged in\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        if(strcmp(response, "User logged out\n") == 0){
            isLoggedIn = false;
            curUser = "";
            }

        c_log.Log(DebugP, "Executing logout operation"); 
    }

    else if(cmd[0] == "show_downloads"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"No users logged in\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing show_downloads operation"); 
    }

    else if(cmd[0] == "stop_share"){
        input_cmd += (" " + curUser);
        if(!isLoggedIn){
            std::cout<<"No users logged in\n";
            return;
        }
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Executing stop_share operation"); 
    }
    
    else {
        get_response(input_cmd, client_socket, response);
        std::cout<<response;
        c_log.Log(DebugP, "Wrong command executed"); 
    }

}

std::vector<std::string> getTrackerInfo(char* path){
    std::fstream trackerInfoFile;
    trackerInfoFile.open(path, std::ios::in);

    std::vector<std::string> res;
    if(trackerInfoFile.is_open()){
        std::string t;
        while(getline(trackerInfoFile, t)){
            res.push_back(t);
        }
        trackerInfoFile.close();
    }
    else{
        std::cout << "Tracker Info file not found.\n";
        exit(-1);
    }
    return res;
}



int main(int argc, char* argv[]){
    if(argc != 3){
        std::cout << "Give arguments as <peer IP:port> and <tracker info file name>\n";
        return -1;
    }

    std::string peerInfo = argv[1];
    std::string tmp; 
    std::stringstream ss(peerInfo);
    std::vector<std::string> peerdetails;

    while(getline(ss, tmp, ':'))
        peerdetails.push_back(tmp);

    std::string peer_ip = peerdetails[0];
    int peer_port = stoi(peerdetails[1]);
    // string trackerInfoFilename = argv[2];

    // int port;
    // std::cout<<"Enter port number: ";
    // std::cin>>port;

    std::vector<std::string> trackerInfo = getTrackerInfo(argv[2]);
    tracker1_ip = trackerInfo[0];
    tracker1_port = stoi(trackerInfo[1]);
    tracker2_ip = trackerInfo[2];
    tracker2_port = stoi(trackerInfo[3]);


    //Running server on thread and calling server function
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_function, &peer_port);

    struct Client client = client_constructor(AF_INET, SOCK_STREAM, 0, tracker1_port, INADDR_ANY);
    // char tracker_ip[] = "127.0.0.1";
    curTracker = client;


    connect_to_server(&client, &tracker1_ip[0]);
    


    //ignoring any inputs 
    // std::cin.ignore();

    std::vector<std::thread> DUThreads;

    while(true){
        // check the tracker is alive or not 
        // char dummy_resp[WORD_SIZE];
        // memset(dummy_resp,0,WORD_SIZE);
        // get_response("keepalive", curTracker.socket, dummy_resp);
        

        std::string tmp, input_cmd;
        std::cout<<"Enter command: ";
        std::getline(std::cin,input_cmd);
        if(input_cmd.size()==0)
            continue;
        
        handleCMD(input_cmd, curTracker.socket, peer_port, DUThreads);
    }

    for(auto i=DUThreads.begin(); i!=DUThreads.end(); i++){
        if(i->joinable()) i->join();
    }

    pthread_join(server_thread, NULL);
    close(curTracker.socket);

    return 0;
}