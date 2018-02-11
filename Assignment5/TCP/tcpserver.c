/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

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
  char* ipaddress;
  int portno;
  int fd;
}user;

user user_info[5];

void setUserfd(int port,int f)
{
    int i;
    for(i = 0; i < 5; i++)
    {
        if(user_info[i].portno == port)user_info[i].fd = f;
    }
}

char * NameLookUp(int f)
{
     int i;
    for(i = 0; i < 5; i++)
    {
        if(user_info[i].fd == f) return user_info[i].name;
    }
}

int main(int argc, char **argv)
{
    int parentfd; /* parent socket */
    int childfd; /* child socket */
    int result;
    fd_set readset;
    int max_fd;
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
    char buf[BUFSIZE]; /* message buffer */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 500000;

    /* 
    * check command line arguments 
    */
    if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
    }
    portno = atoi(argv[1]);

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

    /* 
    * listen: make this socket ready to accept connection requests 
    */
    if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen");
    printf("Server Running ....\n");
    /* 
    * main loop: wait for a connection request, echo input line, 
    * then close connection.
    */

    FD_ZERO(&readset);
    FD_SET(0,&readset);
    FD_SET(parentfd,&readset);
    max_fd = parentfd; 

    while (1) 
    {
        result = select(max_fd + 1,&readset,NULL,NULL,NULL);
        if(result == -1) error("error occured in select");
        int i;
        for(i = 0; i < max_fd; i++)
        {
            if(FD_ISSET(i,&eadset))
            {
                //Is server socket?
                //yes
                if(i == parentfd)
                {
                    clientlen = sizeof(clientaddr);
                    childfd = accept(parentfd,(struct sockaddr*)&clientaddr,&clientlen);
                    if(childfd == -1) error("accept");
                    else 
                    {
                        FD_SET(childfd,&readset);
                        if(childfd > max_fd) max_fd = childfd;
                        setUserfd(clientaddr -> sin_port,childfd);
                    }
                }
                // not a server socket
                else
                {
                    //is stdin?
                    if(i == 0)
                    {
                        
                    }
                    //not an stdin
                    else
                    {
                        //read the message and display
                        n = recv(i, buf, BUFSIZE,0);
                        if (n < 0) 
                        error("ERROR reading from socket");
                        printf("Message from %s : %s", NameLookUp(i), buf);
                    }
                }
            }
        }

    }
}
