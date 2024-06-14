#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <regex.h>
#include <pthread.h>

#define MAXCLIENTS 20
#define MAXBUF 1024
#define MAX_KEYS 10

void usage(){
    printf("syntax : ./server <port>\n");
    printf("sample : ./server 8080\n");
}
/*client struct*/
typedef struct{
    int SockFd;
    struct sockaddr_in Addr;
} client_t;

client_t Clients[MAXCLIENTS];

pthread_mutex_t ClientMutex = PTHREAD_MUTEX_INITIALIZER;

/*key, value struct*/
typedef struct {
    char key[MAXBUF];
    char value[MAXBUF];
} KeyValue;

KeyValue SetKeyVal[MAX_KEYS];
int ExistOffset = 0;

void KeyValSet(int ClientSock, char* key, char* value){
    int KeyFlag = 0;
    /*Check exist*/
    for(int i=0; i<ExistOffset; i++){
        if(strcmp(SetKeyVal[i].key, key) == 0){
            strncpy(SetKeyVal[i].value, value, MAXBUF); /*update value*/
            KeyFlag = 1;
            break;
        }
    }
    /*New key value fill*/
    if((KeyFlag==0) && (ExistOffset < MAX_KEYS)){
        strncpy(SetKeyVal[ExistOffset].key, key, MAXBUF);
        strncpy(SetKeyVal[ExistOffset].value, value, MAXBUF);
        ExistOffset++;
    }

    char Response[] = "+OK\r\n";
    pthread_mutex_lock(&ClientMutex);
    send(ClientSock, Response, strlen(Response), 0);
    pthread_mutex_unlock(&ClientMutex);
}

void KeyValGet(int ClientSock, char* key){
    int ExistFlag = 0;
    char Response[MAXBUF] = {0,};
    char NoValResponse[] = "$-1\r\n";
    for(int i=0; i< ExistOffset; i++){
        if(strcmp(SetKeyVal[i].key, key) == 0){
            snprintf(Response, MAXBUF+6, "$%ld\r\n%s\r\n", strlen(SetKeyVal[i].value), SetKeyVal[i].value);
            ExistFlag = 1;
            break;
        }
    }
    /*Send response*/
    pthread_mutex_lock(&ClientMutex);
    if(ExistFlag){
        send(ClientSock, Response, strlen(Response), 0);
    }
    else{
        send(ClientSock, NoValResponse, strlen(NoValResponse), 0);
    }
    pthread_mutex_unlock(&ClientMutex);
}
void HandleRegEx(int ClientSock, char* RcvBuf){
    /*KEY&VALUE variable*/
    char key[MAXBUF] = {0, }; 
    char value[MAXBUF] = {0, };
    /*regular expression variable*/
    regex_t SetRegEx;
    regex_t GetRegEx;
    regmatch_t MatchSet[3];
    regmatch_t MatchGet[2];
    
    regcomp(&SetRegEx, "^[ ]*[Ss][Ee][Tt][ ]+([^ ]+)[ ]+([^ ]+)[ ]*$", REG_EXTENDED);
    regcomp(&GetRegEx, "^[ ]*[Gg][Ee][Tt][ ]+([^ ]+)[ ]*$", REG_EXTENDED);
    
    /*Regular expression*/
    if(regexec(&SetRegEx, RcvBuf, 3, MatchSet, 0) == 0){
        snprintf(key, MatchSet[1].rm_eo - MatchSet[1].rm_so + 1, "%s", RcvBuf + MatchSet[1].rm_so);
        snprintf(value, MatchSet[2].rm_eo - MatchSet[2].rm_so + 1, "%s", RcvBuf + MatchSet[2].rm_so);
        KeyValSet(ClientSock, key, value);
    }
    else if(regexec(&GetRegEx, RcvBuf, 2, MatchGet, 0) == 0){
        snprintf(key, MatchGet[1].rm_eo - MatchGet[1].rm_so + 1, "%s", RcvBuf + MatchGet[1].rm_so);
        KeyValGet(ClientSock, key);
    }
    else{
        pthread_mutex_lock(&ClientMutex);
        send(ClientSock, "Invalid Command\r\n", strlen("Invalid Command\r\n"), 0);
        pthread_mutex_unlock(&ClientMutex);
    }
    regfree(&SetRegEx);
    regfree(&GetRegEx);
}

void* ClientRcvSnd(void* pClient){
    int ClientIDX = *(int*)pClient;
    char RcvBuf[MAXBUF];
    int ret;

    free(pClient);
    while(1){
        memset(RcvBuf, 0, sizeof(RcvBuf));
        ret = recv(Clients[ClientIDX].SockFd, RcvBuf, MAXBUF, 0);
        if(ret>0){
            RcvBuf[ret-1] = '\0';
            HandleRegEx(Clients[ClientIDX].SockFd, RcvBuf);
        }
        else if(ret==0){
            pthread_mutex_lock(&ClientMutex);
            Clients[ClientIDX].SockFd = -1;
            pthread_mutex_unlock(&ClientMutex);
            break;
        }
        else{
            printf("Error code: %d\n", ret);
            printf("Receive fail!!!\n");
            pthread_mutex_lock(&ClientMutex);
            Clients[ClientIDX].SockFd = -1;
            pthread_mutex_unlock(&ClientMutex);
            break;
        }
    }
}

int main(int argc, char* argv[]){
    /*check main parameter*/
    if(argc!=2){
        usage();
        return -1;
    }
    /*main start!!!*/
    int ServerSock;
    int NewSock;
    int PortNum;
    struct sockaddr_in ServerAddr;
    struct sockaddr_in ClientAddr;
    int AddrLen = sizeof(ClientAddr);
    pthread_t ThreadNum;
    
    for(int i=0; i<MAXCLIENTS; i++){
        Clients[i].SockFd = -1;
    }
    /*Create Socket code*/
    /*ipv4, TCP*/
    ServerSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(ServerSock < 0){
        printf("FAIL to create socket\n");
        exit(1);
    }

    /*Allocated ip address, port number to socket*/
    memset(&ServerAddr, 0, sizeof(ServerAddr));
    PortNum = atoi(argv[1]);
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(PortNum);
    ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(ServerSock, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr)) < 0){
        printf("FAIL to binding\n");
        shutdown(ServerSock, SHUT_RDWR);
        exit(1);
    }

    if(listen(ServerSock, 5) < 0){
        printf( "FAIL to listening\n");
        shutdown(ServerSock, SHUT_RDWR);
        exit(1);
    }

    while(1){
        NewSock = accept(ServerSock, (struct sockaddr*)&ClientAddr, (socklen_t*)&AddrLen);
        if(NewSock<0){
            printf("FAIL to accept\n");
            shutdown(ServerSock, SHUT_RDWR);
            return -1;
        }

        /*find client in CLIENTS[i]*/
        int ClientIDX = -1;
        pthread_mutex_lock(&ClientMutex);
        for(int i=0; i<MAXCLIENTS; i++){
            if(Clients[i].SockFd==-1){
                Clients[i].SockFd = NewSock;
                Clients[i].Addr = ClientAddr;
                ClientIDX = i;
                break;
            }
        }
        pthread_mutex_unlock(&ClientMutex);

        if(ClientIDX == -1){
            shutdown(NewSock, SHUT_RDWR);
            continue;
        }

        int* pClient = malloc(sizeof(int));
        *pClient = ClientIDX;
        if(pthread_create(&ThreadNum, NULL, ClientRcvSnd, pClient) != 0){
            printf("FAIL create thread!!!\n");
            free(pClient);
        }
        pthread_detach(ThreadNum);
    }
    shutdown(ServerSock, SHUT_RDWR);

    return 0;
}