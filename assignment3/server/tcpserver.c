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
#include <openssl/md5.h>
#include <math.h>

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
    struct in_addr sin_addr;	 /* IP address */
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

int main(int argc, char **argv) {
    int parentfd; /* parent socket */
    int childfd; /* child socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
    char buf[BUFSIZE]; /* message buffer */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */
    char* filename; /* file name*/
    char* size_in_string; /* size of file in string*/
    int size_of_file; /* size of file integer */
    char* file; /* file data */
    MD5_CTX mdContext; /* MD5 context data */
    unsigned char checksum[MD5_DIGEST_LENGTH+1]; /* MD5 checksum */
    FILE* fd;
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
    clientlen = sizeof(clientaddr);
    while (1) {

        /* 
         * accept: wait for a connection request 
         */
        childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
        if (childfd < 0) 
            error("ERROR on accept");

        /* 
         * gethostbyaddr: determine who sent the message 
         */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
                sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        printf("server established connection with %s (%s)\n", 
                hostp->h_name, hostaddrp);

        /* 
         * read: read input string from the client
         */
        bzero(buf, BUFSIZE);
        n = read(childfd, buf, BUFSIZE);
        if (n < 0) 
            error("ERROR reading from socket");
        //printf("server received %d bytes: %s", n, buf);

        /*
         * Parse input for file name and size of file
         */
        filename = strtok(buf,":");
        size_in_string = strtok(NULL,":");
        size_of_file = atoi(size_in_string);
        printf("Name of file: %s.\nSize Of file: %s.\n",filename,size_in_string);

        fd = fopen(filename,"w+");        
        /*
         *  Receive file from client
         */
        bzero(buf,BUFSIZE);
        MD5_Init(&mdContext);
        int i = ceil(size_of_file/((double)BUFSIZE));
        int chunks = i;
        //int i = 63;
        while(n = recv(childfd,buf,BUFSIZE,0)){
            if (n < 0) 
                error("ERROR reading from socket");
            if(n == 0)
                error("No file chunk received");
            //if(n > 0)
            //    printf("File chunk %d recieved.\n",i+1);
            fwrite(buf,n,1,fd);
            bzero(buf,BUFSIZE);
        }
        printf("Received file in %d chunks.\n",chunks );
        /*
         *  Compute MD5checksum 
         */
        while(feof(fd) == 0){
            fread(buf,BUFSIZE,1,fd);
            MD5_Update(&mdContext,buf,BUFSIZE);
            if (n < 0) 
                error("ERROR writing to socket");
            bzero(buf,BUFSIZE);
            i++;
        }
        fclose(fd);
        MD5_Final(checksum,&mdContext);
        checksum[MD5_DIGEST_LENGTH] = '\0';
        /* 
         * write: echo the input string back to the client 
         */
        n = write(childfd, checksum, MD5_DIGEST_LENGTH+1);
        if (n < 0) 
            error("ERROR writing to socket");

        close(childfd);
    }
}
