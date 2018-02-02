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

void inttostr(char* array,int index,int value){
    array[index] = (value >> 24) & 0xFF;
    array[index+1] = (value>>16) & 0xFF;
    array[index+2] = (value>>8) & 0xFF;
    array[index+3] = value & 0xFF;
    return;
}

int strtoint(char* array,int index){
    return ((int)array[index])<<24 + ((int)array[index+1])<<16 + ((int)array[index+2])<<8 + (int)array[index+3];
}

_Bool checkACK(char* m,char* ack){
    return (m[0]^ack[0])|(m[1]^ack[1])|(m[2]^ack[2])|(m[3]^ack[3]);
}

void createACK(char* ack,char* buf){
    strncpy(ack,buf,4);
    return;
}

void sendReliableUDP(int sockfd, char* buf,struct sockaddr_in serveraddr){
    int n;
    char recvbuf[BUFSIZE];
    while(1){
        n = sendto(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
        if (n < 0) 
            error("ERROR writing to socket");
        bzero(recvbuf,BUFSIZE);
        n = recvfrom(sockfd, recvbuf, BUFSIZE,0,(struct sockaddr *)&serveraddr,(socklen_t*)sizeof(serveraddr));
        if (n < 0){ 
            error("ERROR reading from socket");
        }
        else{
            if(checkACK(buf,recvbuf) == 0)
                break;
        }
    }
    bzero(buf,BUFSIZE);
    //strcpy(buf,recvbuf);
    return;
}

void recvReliableUDP(int sockfd, char* buf, struct sockaddr_in serveraddr){
    int n,serverlen = sizeof(serveraddr);
    char ack[BUFSIZE];
    n = recvfrom(sockfd,buf,BUFSIZE,0,(struct sockaddr*)&serveraddr,&serverlen);
    if(n < 0)
        error("Error reading from the socket");

    createACK(ack,buf);
    n = sendto(sockfd,ack,BUFSIZE,0,(struct sockaddr*)&serveraddr,serverlen);
    if(n<0)
        error("ERROR writing in server");
}
    

void setSequenceNumber(char* buf,int index){
    inttostr(buf,0,index);
}

void setMessageSize(char* buf,int size){
    inttostr(buf,4,size);
}

int main(int argc, char **argv) {
    int sockfd; /* socket */
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
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
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
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    printf("Server Running ....\n");
    while (1) {
        /* 
         * read: read input string from the client
         */
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE,0,(struct sockaddr*) &clientaddr,(socklen_t*)&clientlen);
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
        while(--i){
            n = read(sockfd,buf,BUFSIZE); 
            MD5_Update(&mdContext,buf,BUFSIZE);
            if (n < 0) 
                error("ERROR reading from socket");
            if(n == 0)
                error("No file chunk received");
            //if(n > 0)
            //    printf("File chunk %d recieved.\n",i+1);
            fwrite(buf,BUFSIZE,1,fd);
            bzero(buf,BUFSIZE);
        }
        printf("Received file in %d chunks.\n",chunks );
        
        fclose(fd);
        /*
         *  Compute MD5checksum 
         */
        MD5_Final(checksum,&mdContext);
        checksum[MD5_DIGEST_LENGTH] = '\0';
        /* 
         * write: echo the input string back to the client 
         */
        n = write(sockfd, checksum, MD5_DIGEST_LENGTH+1);
        if (n < 0) 
            error("ERROR writing to socket");

        close(sockfd);
    }
}
