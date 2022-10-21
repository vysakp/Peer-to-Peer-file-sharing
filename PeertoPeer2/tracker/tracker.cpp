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
#include <unordered_map>
#include <unordered_set>

//Datastuctures 
struct User {
    std::string user_id;
    std::string user_pwd;
    std::string server_ip;
    int port;
    bool is_live;
    std::unordered_map<std::string,std::string> down_status;
    User(){

    }
    User(std::string _uid, std::string _upwd){
        user_id = _uid;
        user_pwd = _upwd;
        is_live = false;
    }

};


struct File{

    std::string file_path;
    int no_of_chunks;
    int size_of_last_chunk;
    std::vector<std::string> chunk_sha;
    File(){

    }
    File(std::string _fpath){
        file_path = _fpath;
    }

};

struct Group {
    std::string group_id;
    std::string owner_id;
    std::unordered_set<std::string> members;
    std::unordered_set<std::string> requests;
    std::unordered_map<std::string,std::unordered_set<std::string>> files;
    Group(){
    }
    Group(std::string _gid, std::string _oid){
        group_id = _gid;
        owner_id = _oid;
    }
};



//map of all users 
std::unordered_map<std::string, User> AllUsers;

//map of all groups
//gourpuuid -> Group
std::unordered_map<std::string, Group> AllGroups;

//map of all files
std::unordered_map<std::string, File> AllFiles;

//map of user to group list
//user->grouplist
std::unordered_map<std::string, std::unordered_set<std::string>> UserGroupMap;


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

bool create_new_user(std::string user_id, std::string user_pwd){
    if(AllUsers.find(user_id)==AllUsers.end()){
        User* new_user = new User(user_id, user_pwd);
        AllUsers[user_id] = *new_user;
        new_user->is_live = true;
        return true;
    }
    else
        return false;

}

int add_group(std::string user_id, std::string group_id){
    std::string group_uuid = user_id + "_" + group_id;

     if(AllGroups.find(group_id)==AllGroups.end()){
        //given group doesn't exist
        Group *new_group = new Group(group_id, user_id);
        //adding owner to memeber list
        new_group->members.insert(user_id);

        UserGroupMap[user_id].insert(group_id);
        AllGroups[group_id] = *new_group;
        return 0;
     }
     else{
        //group exists
        return -1;
     }
    
}

int join_group(std::string cur_usr, std::string group){

    if(AllGroups.find(group)==AllGroups.end()){
        //group does not exists
        return -1;
    }
    else{
        Group *join_grp = &AllGroups[group];
        if(join_grp->requests.find(cur_usr)!=join_grp->requests.end()){
            //request already exists
            return 0;
        }
        else if(join_grp->members.find(cur_usr)!=join_grp->members.end())
            return -2;
        else{
            join_grp->requests.insert(cur_usr);
            return 1;
        }

    }
}


int list_group_req(std::string cur_usr, std::string group){
    if(AllGroups.find(group)==AllGroups.end()){
        //group does not exists
        return -1;
    }
    else{
        Group *grp = &AllGroups[group];
        if(grp->owner_id!=cur_usr)
            return 0;
        else
            return 1;
    }

}

int accept_group_req(std::string cur_usr, std::string peer_usr, std::string group){
    if(AllGroups.find(group)==AllGroups.end()){
        //group does not exists
        return -1;
    }
    else{
        Group *grp = &AllGroups[group];
        if(grp->owner_id!=cur_usr)
            return 0;
        else{
            if(grp->requests.find(peer_usr)!=grp->requests.end()){
                grp->requests.erase(grp->requests.find(peer_usr));
                grp->members.insert(peer_usr);
                return 1;
            }
            else
                return -2;
        }
    }
}

int leave_group(std::string cur_usr, std::string group){
    //TODO: check what to do if the owner is leaving the group
    if(AllGroups.find(group)==AllGroups.end()){
        //group does not exists
        return -1;
    }
    else{
        Group *grp = &AllGroups[group];
        if(grp->members.find(cur_usr)!=grp->members.end()){
            //request already exists
            grp->members.erase(grp->members.find(cur_usr));
            return 1;
        }
        else{
            return 0;
        }
    }

}

int upload_file(std::string cur_usr, std::string file, std::string group){
    if(AllGroups.find(group)==AllGroups.end()){
        //group does not exists
        return -1;
    }
    else{
        Group *grp = &AllGroups[group];
        if(grp->members.find(cur_usr)==grp->members.end()){
            //not a memeber of the group
            return 0;
            }
        else{
            //first entry
            std::string base_filename = file.substr(file.find_last_of("/\\") + 1);
            grp->files[base_filename].insert(cur_usr);
            // std::unordered_set<std::string> usrs;
            // usrs.insert(cur_usr);
            // grp->files[file] = usrs ;
            return 1;
        }

    }

}


int list_files(std::string cur_usr, std::string group){
    if(AllGroups.find(group)==AllGroups.end()){
        //group does not exists
        return -1;
    }
    else{
        Group *grp = &AllGroups[group];
        if(grp->members.find(cur_usr)==grp->members.end()){
            //not a memeber of the group
            return 0;
            }
        else{
            return 1;
        }

    }
}

int download_file(std::string cur_usr, std::string group, std::string src_file, std::string des_loc){
    if(AllGroups.find(group)==AllGroups.end()){
        //group does not exists
        return -1;
    }
    else {
        Group *grp = &AllGroups[group];
        if(grp->members.find(cur_usr)==grp->members.end()){
            //Not a member of the group
            return 0;
        }
        else if(grp->files.find(src_file)==grp->files.end()){
            //file not present in the group
            return -2;
        }
        else{
            //is member and group contains the file 
            //sent the details of the users that have the files 
            return 1;
        }
    }
}
    // if(UserGroupMap.find(user_id) != UserGroupMap.end()){
    //     //user already have some groups 
    //     if(AllGroups.find(group_id)==AllGroups.end()){
    //         //given group doesn't exist
    //         Group *new_group = new Group(group_id, user_id);

    //         //adding owner to memeber list
    //         new_group->members.insert(user_id);

    //         UserGroupMap[user_id].insert(group_id);
    //         AllGroups[group_uuid] = *new_group;
    //         return 0;
    //     }
    //     else{
    //         //given group exists;
    //         return -1;
    //     }
    // }
    // else{
        //user creating group for the first time

    //     Group *new_group = new Group(group_id, user_id);
    //     new_group->members.insert(user_id);

    //     std::unordered_set<std::string> group_set;
    //     group_set.insert(group_id);

    //     UserGroupMap[user_id].insert(group_id);
    //     AllGroups[group_uuid] = *new_group;
    //     return 1;
    // }


void* handle_connection(int  client_socket){

    // int* client_socket = (int *)arg;
    std::string cur_user = "";
    const char* c_user;
    while(true)
    {   
        
        char client_req[1024] = {0};
        // size_t size;

        // size = read(client_socket, client_req, 1024);

        

        if(read(client_socket, client_req, 1024)<=0){
            close(client_socket);
            break;
        }
        

        std::string tmp, input = client_req;
        std::stringstream ss(input);
        std::vector<std::string> input_cmd;
        while(ss>>tmp){
                input_cmd.push_back(tmp);
        }
        
        // std::cout<<input_cmd[0]<<'\n';

        if(input_cmd[0] == "create_user"){
            if(input_cmd.size()!=3)
                write(client_socket, "Wrong format\nPlease try: create_user <user_id> <password>\n", 58);

            else if(create_new_user(input_cmd[1],input_cmd[2]))
            {
                write(client_socket, "New user created\n", 17);
                std::cout<<"New user "<<input_cmd[1]<<" created"<<'\n';
                }
            else
                write(client_socket, "User already exists\n", 20);
        }

        else if(input_cmd[0] == "login"){
            //Adding two args <server ip> and <server port> at user side
            if(input_cmd.size()!=5)
                write(client_socket, "Wrong format\nPlease try: login <user_id> <password>\n", 52);
            else if(AllUsers.find(input_cmd[1])==AllUsers.end())
                write(client_socket, "User doesn't exist\n", 19);
            else if(AllUsers[input_cmd[1]].user_pwd!=input_cmd[2])
                write(client_socket, "Incorrect Password\n", 28);
            else if(AllUsers[input_cmd[1]].is_live)
                write(client_socket, "User have an active session\n", 28);
            else{
                AllUsers[input_cmd[1]].is_live = true;
                cur_user += input_cmd[1];
                AllUsers[input_cmd[1]].server_ip = input_cmd[3];
                AllUsers[input_cmd[1]].port = stoi(input_cmd[4]);
                write(client_socket, "Login successful\n", 17);
                std::cout<<"User "<<cur_user<<" Logged in"<<'\n';
                
            }
            
        }


        else if(input_cmd[0] == "create_group"){
            
            if(input_cmd.size()!=2)
                write(client_socket, "Wrong format\nPlease try: create_group <group_id>\n", 49);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to create a group\n", 32);
            else {
                int res = add_group(cur_user, input_cmd[1]);
                if(res < 0 )
                    write(client_socket, "Group id already exists\n", 24);
                else{
                    write(client_socket, "Group created\n", 15);
                    std::cout<<"Group "<<input_cmd[1]<<" created by user "<<cur_user<<'\n';
                    }
            }

        }


        else if(input_cmd[0] == "join_group"){
            if(input_cmd.size()!=2)
                write(client_socket, "Wrong format\nPlease try: join_group <group_id>\n", 47);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to join a group\n", 32);
            else {
                int res = join_group(cur_user,input_cmd[1]);
                if(res==-1)
                    write(client_socket, "Group does not exists\n", 32);    
                else if(res==0)
                    write(client_socket, "Request in pending state\n", 32);
                else if(res==-2)
                     write(client_socket, "Already a member of the group\n", 32);
                else{
                    write(client_socket, "Request sent\n", 32);
                    std::cout<<"Group "<<input_cmd[1]<<" join request send by user "<<cur_user<<'\n';
                    }
            }
        }


        else if(input_cmd[0] == "leave_group"){
            if(input_cmd.size()!=2)
                write(client_socket, "Wrong format\nPlease try: leave_group <group_id>\n", 48);
            //Continue here ...
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to join a group\n", 32);
            else{
                int res = leave_group(cur_user, input_cmd[1]);
                if(res == -1){
                    write(client_socket, "Group does not exists\n", 32);
                }
                else if(res == 0)
                    write(client_socket, "Not a member of the group\n", 32);
                else{
                    write(client_socket, "You left the group\n", 32);
                    std::cout<<cur_user<<" left group "<<input_cmd[1]<<'\n';
                    }
            }
                
        }


        else if(input_cmd[0] == "list_requests"){
            if(input_cmd.size()!=2)
                write(client_socket, "Wrong format\nPlease try: list_requests <group_id>\n", 49);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to get requests\n", 32);
            else
            { 
                int res = list_group_req(cur_user, input_cmd[1]);

                if(res==-1)
                    write(client_socket, "Group does not exists\n", 32);
                else if(res==0)
                    write(client_socket, "You are not the owner to this group\n", 32);
                else if(AllGroups[input_cmd[1]].requests.empty())
                    write(client_socket, "No pending requests\n", 32);
                else {

                    std::string all_reqs = "";

                    for(auto req : AllGroups[input_cmd[1]].requests){
                        all_reqs += (req+ '\n');

                    write(client_socket, &all_reqs[0], all_reqs.size());
                }

                }

            }
        }


        else if(input_cmd[0] == "accept_request"){
            if(input_cmd.size()!=3)
                write(client_socket, "Wrong format\nPlease try: accept_request <group_id> <user_id>\n", 61);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to accept requests\n", 32);
            else{
                
                int res = accept_group_req(cur_user, input_cmd[2], input_cmd[1]);
                switch(res){
                    case -2: 
                        write(client_socket, "Request not present\n", 32);
                        break;
                    case -1:
                        write(client_socket, "Group does not exists\n", 32);
                        break;
                    case 0:
                        write(client_socket, "You are not the owner to this group\n", 32);
                        break;
                    case 1:
                        write(client_socket, "Request accepted\n", 32);
                        std::cout<<cur_user<<" accepted  "<<input_cmd[2]<<" to group "<<input_cmd[1]<<'\n';
                        break;
                    default:
                        write(client_socket, "Welcome to Narnia\n", 20);
                }
            }
        }

        else if(input_cmd[0] == "list_groups"){
            if(input_cmd.size()!=1)
                write(client_socket, "Wrong format\nPlease try: list_groups\n", 37);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to get requests\n", 32);
            else{
                std::string all_groups = "";
                for(auto group_map : AllGroups){
                    all_groups += (group_map.first+ '\n');
                }
                write(client_socket, &all_groups[0], all_groups.size());

            }
        }

        else if(input_cmd[0] == "list_files"){
            if(input_cmd.size()!=2)
                write(client_socket, "Wrong format\nPlease try: list_files <group_id>\n", 48);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to get requests\n", 32);
            else{
                int res = list_files(cur_user, input_cmd[1]);
                if(res==-1){
                    write(client_socket, "Group does not exists\n", 32); 
                }
                else if(res==0){
                    write(client_socket, "Please join the group to get details\n", 32); 
                }
                else{
                    std::string all_files = "";
                    for(auto file : AllGroups[input_cmd[1]].files){
                        // std::string base_filename = files.first.substr(file.first.find_last_of("/\\") + 1);
                        all_files += (file.first+ '\n');

                        }


                    write(client_socket, &all_files[0], all_files.size());
                
            }
            // write(client_socket, "Input is list_files", 20);
        }
        }


        else if(input_cmd[0] == "upload_file"){
            if(input_cmd.size()!=3)
                write(client_socket, "Wrong format\nPlease try: upload_file <file_path> <group_id>\n", 61);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to accept requests\n", 32);
            else{
                int res = upload_file(cur_user, input_cmd[1], input_cmd[2]);
                if(res==-1){
                    write(client_socket, "Group does not exists\n", 32); 
                }
                else if(res==0){
                    write(client_socket, "Please join the group to upload\n", 32); 
                }
                else{
                    std::string res_msg = "File uploaded\n";
                    write(client_socket, res_msg.c_str(), res_msg.size());
                    std::string base_filename = input_cmd[1].substr(input_cmd[1].find_last_of("/\\") + 1);

                    File new_file = File(input_cmd[1]);
                    AllFiles[base_filename] = new_file;
                    std::cout<<cur_user<<" uploaded "<<base_filename<<" to group "<<input_cmd[2]<<'\n';


                }
            }   
        }


        else if(input_cmd[0] == "download_file"){
            if(input_cmd.size()!=4)
                write(client_socket, "Wrong format\nPlease try: download_file <group_id> <file_name> <destination_path>\n", 81);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live)
                write(client_socket, "Please log in to accept requests\n", 32);
            else{
                std::string base_filename = input_cmd[2].substr(input_cmd[2].find_last_of("/\\") + 1);
                int res = download_file(cur_user, input_cmd[1], base_filename, input_cmd[3]);

                if(res == -1)
                    write(client_socket, "Group does not exists\n", 32);
                else if(res==0){
                    write(client_socket, "Please join the group to upload\n", 32); 
                }
                else if(res==-2){
                    write(client_socket, "Group doesn't have given file\n", 32);
                }
                else{
                    std::string all_users = "User list\n";
                    for(auto usr: AllGroups[input_cmd[1]].files[input_cmd[2]]){
                        if(AllUsers[usr].is_live)
                            all_users += (usr + " " + AllUsers[usr].server_ip+ " " + std::to_string(AllUsers[usr].port) +'\n');
                        }
                    

                    write(client_socket, &all_users[0], all_users.size());

                    std::cout<<cur_user<<" downloading "<<base_filename<<" from group "<<input_cmd[1]<<'\n';
                    

                }

            }

            // write(client_socket, "Input is download_file", 20);
        }


        else if(input_cmd[0] == "logout"){
            if(input_cmd.size()!=1)
                write(client_socket, "Wrong format\nPlease try: logout\n", 32);
            else if(cur_user.size()==0||!AllUsers[cur_user].is_live){
                write(client_socket, "User not logged in\n", 32);
            }
            else if(!AllUsers[cur_user].is_live)
                 write(client_socket, "User already logged out\n", 32);

            else{
                AllUsers[cur_user].is_live = false;
                write(client_socket, "User logged out\n", 20);
            }
        }


        else if(input_cmd[0] == "show_downloads"){
            std::string all_status = "";
            for(auto status : AllUsers[cur_user].down_status){
                // std::string base_filename = files.first.substr(file.first.find_last_of("/\\") + 1);
                
                all_status += (status.second + " " + status.first + '\n');

                }


            write(client_socket, &all_status[0], all_status.size());
        }


        else if(input_cmd[0] == "stop_share"){
            write(client_socket, "Input is stop_share", 20);
        }
        else if(input_cmd[0] == "file_update"){
            //file_update <file_loc> <no_of_chunks> <last_chunk_size>
            //custom command hence no need to check for errors
            int no_of_chunks = stoi(input_cmd[2]);
            int last_chunk_size = stoi(input_cmd[3]);
            std::vector<std::string> chunk_sha_map(no_of_chunks);

            AllFiles[input_cmd[1]].no_of_chunks = no_of_chunks;
            AllFiles[input_cmd[1]].size_of_last_chunk = last_chunk_size;
            AllFiles[input_cmd[1]].chunk_sha = chunk_sha_map;



            write(client_socket, "file updated..", 20);
        }
        else if(input_cmd[0] == "chunk_sha_update"){
            //chunk_sha_update <file_name> <chunk_no> <sha1sum>
            //cutsom command hence no need to check for errors
            int chunk_no = stoi(input_cmd[2]);
            AllFiles[input_cmd[1]].chunk_sha[chunk_no] = input_cmd[3];
            // std::cout<<"chunk "<<chunk_no<<" = "<<input_cmd[3]<<'\n';
            write(client_socket, "sha updated..", 20);
            
        }
        else if(input_cmd[0] == "down_status_update"){
            //down_status_update <file_name> <status> <group_id> 
            //cutsom command hence no need to check for errors
            AllUsers[cur_user].down_status[input_cmd[1]] = input_cmd[2];
            // std::cout<<"chunk "<<chunk_no<<" = "<<input_cmd[3]<<'\n';
            write(client_socket, "down_status updated..", 20);
            
        }
        else if(input_cmd[0] == "verify_sha"){
            //verify_sha <file_name> <chunk_no> <chunk_sha> 
            //cutsom command hence no need to check for errors
            int chunk_no = stoi(input_cmd[2]);
            std::string actual_sha = AllFiles[input_cmd[1]].chunk_sha[chunk_no];
            std::string given_sha = input_cmd[3];
            std::string res;
            if(actual_sha == given_sha)
                res = "TRUE";
            else
                res = "FALSE";
            // std::cout<<"chunk "<<chunk_no<<" = "<<input_cmd[3]<<'\n';
            write(client_socket, res.c_str(), res.size());
            
        }
        else {
            write(client_socket, "Invalid command\n", 20);
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
    
    int port = 4029;
    struct Server Tracker = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, port, 20);
    struct sockaddr *address = (struct sockaddr*)&Tracker.address;
    socklen_t address_length = (socklen_t)sizeof(Tracker.address);
    pthread_t exitTread;


    pthread_create(&exitTread, NULL, quit_function, &Tracker.socket);
    // std::thread exitThread(quit_function, Tracker.socket);



    //Vector of threads for each incoming client connection
    std::vector<std::thread> vTreads;
    

    while(true){
        int client_socket;
        
        
        if((client_socket = accept(Tracker.socket, address, &address_length))<0){
            std::cout<<"Accept connection failed..."<<'\n';
            // perror("Tracker failed to accept");
        }

        // printf("C Programming");
        // std::cout<<"hi"<<'\n';
        

        //creating a new thread for the client 
        // pthread_t new_client;
        // pthread_create(&new_client, NULL, handle_connection, (void *)client_socket);
        vTreads.push_back(std::thread(handle_connection, client_socket));
        
        // handle_connection(client_socket);
    }
     for(auto i=vTreads.begin(); i!=vTreads.end(); i++){
        if(i->joinable()) i->join();
    }

    // for(pthread_t client: vTreads){
    //     pthread_join(client, NULL);
    // }

    return 0;
}
