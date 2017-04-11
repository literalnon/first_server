#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <set>
#include <stack>
#include <iterator>
#include <poll.h>

//server

#define BUF_SIZE 256

std::mutex sockets_mutex;
std::mutex client_count_mutex;

std::set<int> sockets;

static const int num_threads = 10;
static int client_count = 0;

bool sock_flag = true;

bool authorization(int sock, std::string & client_name){
    if (sock < 0){
        printf("accept() failed: %d\n", errno);
        return false;
    }

    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    write(sock, "enter name\n", 12);
    read(sock, buf, BUF_SIZE - 1);

    client_name = std::string(buf);
    printf("client %s\n", buf);

    memset(buf, 0, BUF_SIZE);

    write(sock, "enter password\n", 16);
    read(sock, buf, BUF_SIZE - 1);

    if(*buf == 'w'){
        printf("client success connect\n");
        client_count_mutex.lock();
        client_count++;
        client_count_mutex.unlock();
        write(sock, "success connect\n", 16);
        return true;
    }else{
        write(sock, "unsuccess connect\n", 18);
        printf("client unsuccess connect\n");
    }

    return false;
}

void client_func(int newsock){

    std::string client_name;

    if(authorization(newsock, client_name)){
        sockets_mutex.lock();
        sockets.insert(newsock);
        sockets_mutex.unlock();
    }else{
        close(newsock);
        return;
    }

    char buf[BUF_SIZE];

    if (newsock < 0){
        printf("accept() failed: %d\n", errno);
    }

    printf("new client %s\n", client_name.c_str());

    while(1){
        memset(buf, 0, BUF_SIZE);
        read(newsock, buf, BUF_SIZE-1);
        buf[BUF_SIZE] = 0;
        printf("MSG: %s\n", buf);

        if(*buf == '0'){
            printf("close socket \n");

            sockets_mutex.lock();

            auto it = sockets.find(newsock);
            if(it != sockets.end())
               sockets.erase(it);

            sockets_mutex.unlock();

            client_count_mutex.lock();
            client_count--;
            client_count_mutex.unlock();

            close(newsock);
            break;
        }else{

            write(newsock, "OK\n", 4);

            sockets_mutex.lock();
            for(auto sock : sockets)
                if(sock > 0 && sock != newsock){
                    write(sock, client_name.c_str(), strlen(client_name.c_str()));
                    write(sock, buf, BUF_SIZE - 1);
                }
            sockets_mutex.unlock();
        }
    }
}

void add_sock(int sock){

    std::vector<std::thread> client_threads;

    struct sockaddr_in cli_addr;
    int clen = sizeof(cli_addr);

    listen(sock, 1);

    struct timeval tv;

    int newsock;
    fd_set active_fd_set;



    while(1)
    {
        printf("while");
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        FD_ZERO (&active_fd_set);
        FD_SET (sock, &active_fd_set);

        if(select(FD_SETSIZE, &active_fd_set, &active_fd_set, NULL, &tv) > 0){
            newsock = accept(sock, (struct sockaddr * ) &cli_addr, (socklen_t *) &clen);
        }else if(sock_flag){
            continue;
        }else
             break;

        printf("\n");

        if(newsock > 0)
            printf("new client try connect\n");
        else
            break;

        client_threads.push_back(std::thread(client_func, newsock));
    }

    for(auto & i : client_threads)
        i.join();

    close(sock);
}

int main(int argc, char ** argv)
{
    int sock, port;

    struct sockaddr_in serv_addr;


    if (argc < 2){
        fprintf(stderr,"usage: %s <port_number>\n", argv[0]);
        return EXIT_FAILURE;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);


    if (sock < 0){
        printf("socket() failed: %d\n", errno);
        return EXIT_FAILURE;
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));

    port = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        printf("bind() failed: %d\n", errno);
        close(sock);
        return EXIT_FAILURE;
    }

    printf("server lestening %d \n", port);

    std::thread add_sock_thread(add_sock, sock);

    char s[BUF_SIZE];

    while(1){
        memset(s, 0, BUF_SIZE - 1);
        scanf("%c", s);

        if(*s == 'q'){
            sock_flag = false;
            add_sock_thread.join();
            sockets_mutex.lock();
            for(auto sock : sockets)
                    close(sock);
            sockets_mutex.unlock();
            //close(sock);

            break;

        }else
            printf("no\n");
    }
}
