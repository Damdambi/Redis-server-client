#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <regex.h>
#include <pthread.h>

#define MAXBUF 1024

void usage(){
    printf("syntax : ./client <ip> <port>\n");
    printf("sample : ./client 127.0.0.1 8080\n");
}

pthread_mutex_t ClientMutex = PTHREAD_MUTEX_INITIALIZER;
int ClientSock;
struct sockaddr_in ServerAddr;

void* RcvMessage(void* arg){
    int RcvRet = 0;
    char RcvBuf[MAXBUF];
    while(1){
        memset(RcvBuf, 0, sizeof(RcvBuf));
        RcvRet = recv(ClientSock, RcvBuf, MAXBUF-1, 0);
        if(RcvRet > 0){
            printf("%s", RcvBuf);
            continue;
        }
        else if(RcvRet == 0){
            exit(0);
        }
        else{
            printf("receive error\n");
            exit(1);
        }
    }
}
int main(int argc, char* argv[]){

    if(argc!=3){
        usage();
        return -1;
    }
    char SndBuf[MAXBUF] = {0, };
    char CheckExit[MAXBUF] = {0, };
    int ConnectCheck;
    int RcvRet;
    pthread_t ThreadNum;
    /*EXIT regular expresion*/
    regex_t ExitRegEx;
    regmatch_t MatchExit[1];
    regcomp(&ExitRegEx, "^[ ]*[Ee][Xx][Ii][Tt][ ]*$", REG_EXTENDED);
    /*Create socket code*/
    ClientSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(ClientSock < 0){
        printf("FAIL to create socket\n");
        exit(1);
    }

    /*server information*/
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(atoi(argv[2]));
    if(inet_pton(AF_INET, argv[1], &ServerAddr.sin_addr)!=1){
        printf("Improperty IP address\n");
        exit(1);
    }

    ConnectCheck = connect(ClientSock, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr));
    if(ConnectCheck<0){
        printf("FAIL to connect\n");
        shutdown(ClientSock, SHUT_RDWR);
        exit(1);
    }

    if(pthread_create(&ThreadNum, NULL, RcvMessage, NULL)!=0){
        printf("FAIL to create thread!!!\n");
        exit(1);
    }

    pthread_detach(ThreadNum);

    while(1){
        memset(SndBuf, 0, sizeof(SndBuf));
        read(0, SndBuf, 1024);
        /*Check exit to use regular expression*/
        strcpy(CheckExit, SndBuf);
        CheckExit[strlen(CheckExit)-1] = '\0';     
        if(regexec(&ExitRegEx, CheckExit, 1, MatchExit, 0) == 0){
            break;
        }
        
        pthread_mutex_lock(&ClientMutex);
        if(send(ClientSock, SndBuf, strlen(SndBuf), 0)!=strlen(SndBuf)){
            printf("FAIL to send!!!\n");
            continue;
        }
        pthread_mutex_unlock(&ClientMutex);
    }
    regfree(&ExitRegEx);
    shutdown(ClientSock, SHUT_RDWR);
    return 0;
}