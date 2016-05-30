
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

#define MAX_CLIENTS	100

static unsigned int cli_count = 0;  // record the number of clients
static int uid = 10;

/* Client structure */
typedef struct {
    struct sockaddr_in addr;	/* Client remote address */
    int connfd;			        /* Connection file descriptor acts like socket*/ 
    int uid;			        /* Client unique identifier */
    char name[32];			/* Client name */
} client_t;

client_t *clients[MAX_CLIENTS]; // initialize the clients queue

/* Strip CRLF */
void strip_newline(char *s){
    while(*s != '\0'){
        if(*s == '\r' || *s == '\n'){
            *s = '\0';
        }
        s++;
    }
}

/* Add client to queue */
void queue_add(client_t *cl){
    int i;
    for(i=0;i<MAX_CLIENTS;i++){
        if(!clients[i]) {
            clients[i] = cl;
            return;
        }
    }
}

/* Delete client from queue */
void queue_delete(int uid){
    int i;
    for(i=0;i<MAX_CLIENTS;i++){
        if(clients[i]){
            if(clients[i]->uid == uid){
                clients[i] = NULL; // set the structure to null
                return;
            }
        }
    }
}

/* Send message to all clients but the sender '1' indicate the information is secret*/
void send_message(char *s, int uid){
    int i;
    for(i=0;i<MAX_CLIENTS;i++){
        if(clients[i]){
            if(clients[i]->uid != uid){
                send(clients[i]->connfd, s, strlen(s),0);
            }
        }
    }
}

/* Send message to all clients include the client  itself */
void send_message_all(char *s){
    int i;
    for(i=0;i<MAX_CLIENTS;i++){
        if(clients[i]){
//            int len = strlen(s);
//            char newInfo[len + 1];
//            if (isSecret == 0) {
//                sprintf(newInfo, "%s%s", s, "0");
//            } else {
//                sprintf(newInfo, "%s%s", s, "1");
//            }
            write(clients[i]->connfd, s, strlen(s));
        }
     } 
}

/* Send message to sender */
void send_message_self(char *s, int connfd){
//    int len = strlen(s);
//    char newMsg[len + 1];
//    sprintf(newMsg, "%s%s", s, "0");
    write(connfd, s, strlen(s));
}

/* Send list of active clients */
void send_active_clients(int connfd){
    int i;
    char s[64];
    for(i=0;i<MAX_CLIENTS;i++){
        if(clients[i]){
            sprintf(s, "<<CLIENT %d | %s\r\n", clients[i]->uid, clients[i]->name);
            send_message_self(s, connfd);
        }
    }
}


/* Print ip address */
void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
            addr.sin_addr.s_addr & 0xFF,
            (addr.sin_addr.s_addr & 0xFF00)>>8,
            (addr.sin_addr.s_addr & 0xFF0000)>>16,
            (addr.sin_addr.s_addr & 0xFF000000)>>24);
}

void sigchld_handler(int s) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void saveLog(FILE *fp, char *msg) {
    time_t t = time(0); 
    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y/%m/%d %X %A",localtime(&t)); 
    fprintf(fp,"| Date: %s\n", tmp);
    fprintf(fp, "%s\n", msg);  
    fclose(fp);
    return;

}

/* Handle all communication with the client */
void *hanle_client(void *arg){
    FILE *log = fopen("/Users/owner/Desktop/log.txt", "a+");
    char buff_out[1024];
    char buff_in[1024];
    int rlen;

    cli_count++;	// increase the amount
    client_t *cli = (client_t *)arg; // socket client	

    // print information on server side
    printf("<<ACCEPT ");	// alert receive msg
    print_client_addr(cli->addr);
    printf(" REFERENCED BY %d\n", cli->uid);

    sprintf(buff_out, "<<JOIN, HELLO %s\r\n0", cli->name);
    send_message_all(buff_out); // send join message to all clients

    /* Receive input from client */
    while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1)) > 0){
        buff_in[rlen] = '\0';
        buff_out[0] = '\0';
        strip_newline(buff_in);
        /* Ignore empty buffer */
        if(!strlen(buff_in)){
            continue;
        }
        
        int len = strlen(buff_in);   
        printf("%s\n", buff_in); 
        /* Request list, detect the first char */
        if(buff_in[0] == '\\'){
            // get rid of the encryption code
            buff_in[len - 1] = '\0';
            char *command, *param;
            command = strtok(buff_in," ");
            if(!strcmp(command, "\\QUIT")){
                break;
            }else if(!strcmp(command, "\\PING")){
                send_message_self("<<PONG\r\n0", cli->connfd); // response the the test
            }else if(!strcmp(command, "\\NAME")){
                param = strtok(NULL, " "); // new name
                if(param){
                    char *old_name = strdup(cli->name);
                    strcpy(cli->name, param); // assign new name to client
                    sprintf(buff_out, "<<RENAME, %s TO %s\r\n0", old_name, cli->name);
                    free(old_name);
                    send_message_all(buff_out);
                }else{
                    send_message_self("<<NAME CANNOT BE NULL\r\n0", cli->connfd);
                }
            }else if(!strcmp(command, "\\ACTIVE")){
                sprintf(buff_out, "<<CLIENTS %d\r\n0", cli_count);
                send_message_self(buff_out, cli->connfd);
                send_active_clients(cli->connfd);
            }else if(!strcmp(command, "\\HELP")){
                strcat(buff_out, "\\QUIT     Quit chatroom\r\n");
                strcat(buff_out, "\\PING     Server test\r\n");
                strcat(buff_out, "\\NAME     <name> Change nickname\r\n0");
                send_message_self(buff_out, cli->connfd); // send option list to clients
            }else{
                send_message_self("<<UNKOWN COMMAND\r\n0", cli->connfd);
            }
          }else{
            int nameLen = strlen(cli->name);
            char name[nameLen + 4];
            memset(name, '\0', sizeof(name));
            sprintf(name, "[%s]:0", cli->name); // 0 indicate this is a plain text
            send_message(name, cli->uid);

            sleep(300);

            sprintf(buff_out, "%s", buff_in);
            printf("%s\n", buff_out);
            send_message(buff_out, cli->uid);  

            char *whole = (char*)malloc(strlen(name) + strlen(buff_out));
            sprintf(whole, "%s%s", name, buff_out);
            saveLog(log, whole);
        }
        
        memset(buff_in, '\0', sizeof(buff_in));
        memset(buff_out, '\0', sizeof(buff_out));
  }

    /* Close connection and send leave information*/
    close(cli->connfd);
    sprintf(buff_out, "<<LEAVE, BYE %s\r\n0", cli->name);
    send_message_all(buff_out);

    /* Delete client from queue and yeild thread */
    queue_delete(cli->uid);
    printf("<<LEAVE ");
    print_client_addr(cli->addr);
    printf(" REFERENCED BY %d\n", cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char *argv[]){
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    struct sigaction sa;
    pthread_t tid;

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(5000); 

    /* Bind */
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        perror("Socket binding failed");
        return 1;
    }

    /* Listen */
    if(listen(listenfd, 10) < 0){
        perror("Socket listening failed");
        return 1;
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    printf("<[SERVER STARTED]>\n");

    /* Accept clients */
    while(1){
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        /* Check if max clients is reached, if reached, reject and send reject message*/
        if((cli_count+1) == MAX_CLIENTS){
            printf("<<MAX CLIENTS REACHED\n");
            printf("<<REJECT ");
            print_client_addr(cli_addr);
            printf("\n");
            close(connfd);
            continue;
        }

        /* Client settings */
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->addr = cli_addr;  // record the 
        cli->connfd = connfd;  
        cli->uid = uid++; // assign id for the new user
        sprintf(cli->name, "%d", cli->uid);

        queue_add(cli);
        // use thread not fork to create a thread
        pthread_create(&tid, NULL, &hanle_client, (void*)cli); // create a new thread for the new client
        
        /* Reduce CPU usage */
        sleep(1);
    }
}
