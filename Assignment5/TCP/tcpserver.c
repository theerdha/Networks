/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
#define MAX_CLIENTS 1
#define TIMEOUT 20
#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
    unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
    unsigned short int sin_family; /* Address family */
    unsigned short int sin_port;   /* Port number */
    struct in_addr sin_addr;   /* IP address */
    unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
    char    *h_name;        /* official name of host */
    char    **h_aliases;    /* alias list */
    int     h_addrtype;     /* host address type */
    int     h_length;       /* length of address */
    char    **h_addr_list;  /* list of addresses */
}
#endif

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

typedef struct USER
{
    char* name;
    struct sockaddr_in client; 
    int fd;
    int status; // status 0 connection not established, status 1 connection is established 
    clock_t timestamp;
}user;

user user_info[MAX_CLIENTS];

void setUser(struct in_addr IP,unsigned short int port,int f)
{
    int i;
    for(i = 0; i < MAX_CLIENTS; i++)
    {
        if(user_info[i].client.sin_addr.s_addr == IP.s_addr)
        {
            user_info[i].fd = f;
            user_info[i].client.sin_port = port;
            user_info[i].status = 1;
            user_info[i].timestamp = clock();
        }
    }
}

user* NameLookUp(char* username)
{
    int i;
    for(i = 0; i < MAX_CLIENTS; i++)
    {
        if(strcmp(user_info[i].name, username) == 0)  
            return &user_info[i];
    }
}

int main(int argc, char **argv)
{
    int parentfd; /* parent socket */
    int childfd; /* child socket */
    int result;
    fd_set readset;
    int max_fd;
    int clientlen;
    struct sockaddr_in clientaddr;
    int portno; /* port to listen on */
    struct sockaddr_in serveraddr; /* server's addr */
    char buf[BUFSIZE]; /* message buffer */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */
    int i;
    struct timeval tv;
    char* clientips[MAX_CLIENTS];
    int clientports[MAX_CLIENTS];
    struct hostent* client_addr[MAX_CLIENTS];
    char* client_names[MAX_CLIENTS];
    char* receiver;
    char* message;
    user* destination;
    /////////// INITALIZE THE CLIENT IPS AND PORTS ////////////////
    // TODO
    // Initalize the status with zeros
    /* 
     * check command line arguments 
     */
    clientips[0] = "10.145.188.132";
    clientports[0] = 9000;
    client_names[0] = "Theeru";
    

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);
    
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    /* 
     * socket: create the parent socket 
     */
    parentfd = socket(AF_INET, SOCK_STREAM, 0);
    if (parentfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));

    /* this is an Internet address */
    serveraddr.sin_family = AF_INET;

    /* let the system figure out our IP address */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* this is the port we will listen on */
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(parentfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    if (listen(parentfd, MAX_CLIENTS) < 0) /* allow 5 requests to queue up */ 
        error("ERROR on listen");
    printf("Server Running ....\n");
    /* 
     * main loop: wait for a connection request, echo input line, 
     * then close connection.
     */

    // Initialize user data
    for(i = 0;i < MAX_CLIENTS; i++){
        client_addr[i] = gethostbyname(clientips[i]);
        bzero(&(user_info[i].client),sizeof(user_info[i].client)); 
        user_info[i].client.sin_family = AF_INET;    
        bcopy((char*)client_addr[i]->h_addr,(char*) &user_info[i].client.sin_addr.s_addr,client_addr[i]->h_length);
        user_info[i].client.sin_port = htons(clientports[i]);
        user_info[i].name = (char*) malloc(sizeof(char)*20);
        strcpy(user_info[i].name,client_names[i]);
        user_info[i].status = 0;
    }

    // Initialize read set

    max_fd = parentfd; 

    while (1) 
    {
        // Store required fds
        //
        FD_ZERO(&readset); // reset readset
        FD_SET(STDIN_FILENO,&readset); // Adds std input fd
        FD_SET(parentfd,&readset); // Adds sevrer listening fd

        for(i = 0; i < MAX_CLIENTS; i++){
            if(user_info[i].status == 1){
                FD_SET(user_info[i].fd,&readset);
                if(max_fd < user_info[i].fd)
                    max_fd = user_info[i].fd;
            }
        }

        // Moniter the fds
        //
        if((result = select(max_fd + 1,&readset,NULL,NULL,&tv)) == -1)
            error("Error occured in select");

        //Is server socket?
        //yes
        if(FD_ISSET(parentfd,&readset))
        {
            while(errno != EAGAIN && errno != EWOULDBLOCK)
            {
                if((childfd = accept(parentfd,(struct sockaddr*)&clientaddr,&clientlen)) == -1) 
                    perror("error in accept");
                setUser(clientaddr.sin_addr,clientaddr.sin_port,childfd);

            }
        }

        // check client fds
        for(i = 0; i < MAX_CLIENTS ; i++)
        {
            if(user_info[i].status == 1 && FD_ISSET(user_info[i].fd,&readset))
            {  
                bzero(buf,BUFSIZE);
                //read the message and display
                n = recv(user_info[i].fd, buf, BUFSIZE,0);
                if (n < 0) 
                    error("ERROR reading from socket");
                printf("%s/ %s", user_info[i].name, buf);     

            }
        }
        bzero(buf,BUFSIZE);

        if(FD_ISSET(STDIN_FILENO,&readset))
        {
            bzero(buf,BUFSIZE);
            if((n = read(STDIN_FILENO,buf,BUFSIZE)) < 0)
                error("Failed to read from STD input");
            receiver = strtok(buf,"/");
            message = strtok(NULL,"/");
            destination = NameLookUp(receiver);
            if(destination->status == 1){
                if((n = send(destination->fd,buf,BUFSIZE,0)) < 0)
                    perror("Failed to send data to destination");
                if(errno == ECONNRESET){
                    printf("Re Establishing connection\n");
                    if( connect(destination->fd,(const struct sockaddr*)&destination->client,sizeof(destination->client)) < 0)
                        perror("Couldn't establish connection");
                    else{
                        destination->timestamp = clock();
                        destination->status = 1;
                    }
                    if((n = send(destination->fd,buf,BUFSIZE,0)) < 0)
                        perror("Failed to send data to destination");
                }
            }
            else{
                if((childfd = socket(AF_INET,SOCK_STREAM,0) < 0) )
                    perror("Unable to create socket for sending message");
                else
                    destination->fd = childfd;

                if(connect(destination->fd,(const struct sockaddr*)&destination->client,sizeof(destination->client) ) < 0)
                    perror("Couldn't establish connection");
                else{
                    destination->timestamp = clock();
                    destination->status = 1;
                    if((n = send(destination->fd,buf,BUFSIZE,0)) < 0)
                        perror("Failed to send data to destination");
                }
            }
            // How to quit the application
            //
        }

        // RESET the status flags if connection is timedout and close the connection
        for(i = 0; i < MAX_CLIENTS; i++){
            if(user_info[i].status == 1 && ((clock() - user_info[i].timestamp)/CLOCKS_PER_SEC > TIMEOUT)){
                close(user_info[i].fd);
                user_info[i].status = 0;
            }
        }
    }
}
