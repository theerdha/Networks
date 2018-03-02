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
#define ACKSIZE 1024
#define TIMEOUT 1
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

#include "../udpreliable.h"

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
    char buf[BUFSIZE]; /* message buffer */
    char ack[ACKSIZE];
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */
    char* filename; /* file name*/
    char* size_in_string; /* size of file in string*/
    char* no_of_packets_str;
    int no_of_packets;
    int seq;
    int size_of_file; /* size of file integer */
    char* file; /* file data */
    MD5_CTX mdContext; /* MD5 context data */
    unsigned char checksum[MD5_DIGEST_LENGTH+1]; /* MD5 checksum */
    FILE* fd;
    int* seqbuffer;
    struct timeval timeout;
    int expected_seqnumber;
    //vector<int> seqRecv;
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
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO|SO_REUSEADDR, 
            (const char *)&timeout , sizeof(timeout));

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
    clientlen = sizeof(clientaddr);
    while (1) {
        /* 
         * read: read input string from the client
         */
        bzero(buf, BUFSIZE);
        //recvReliableUDP(sockfd,buf,&clientaddr);
		n = recvfrom(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&clientaddr,&clientlen);
        if (n < 0) error("ERROR in recvfrom1"); 
        
        
        /*
         * Parse input for file name and size of file
         */
        filename = strtok(buf+8,":");
        size_in_string = strtok(NULL,":");
        size_of_file = atoi(size_in_string);
        no_of_packets_str = strtok(NULL,":");
        no_of_packets = atoi(no_of_packets_str);
        seq = strtoint(buf,0);
        printf("SeqNo %d\nName of file: %s.\nSize Of file: %s.\nNo Of Packets: %s\n",seq,filename,size_in_string,no_of_packets_str);
        int i;
        
        expected_seqnumber = 1;

        fd = fopen(filename,"w+");        
        /*
         *  Receive file from client
         */
        bzero(buf,BUFSIZE);
        MD5_Init(&mdContext);
        i = 0;
        
        while(i < no_of_packets){
            bzero(buf,BUFSIZE);
            n = recvfrom(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&clientaddr,&clientlen);
            if (n < 0) error("ERROR in recvfrom"); 
            seq = strtoint(buf,0);
           // printf("seq = %d\n", seq);
           

            if(seq < expected_seqnumber)
            {
                bzero(ack,ACKSIZE);
                createdupACK(ack,expected_seqnumber - 1);
                n = sendto(sockfd,ack,ACKSIZE,0,(struct sockaddr *)&clientaddr,sizeof(clientaddr));
                if(n<0) error("ERROR writing in server3");
                printf("duplicate packet:%d received\n",seq);
            }

            else if(seq > expected_seqnumber)
            {
                bzero(ack,ACKSIZE);
                //create ack with expected seqno -1
                createdupACK(ack,expected_seqnumber - 1); 
                n = sendto(sockfd,ack,ACKSIZE,0,(struct sockaddr *)&clientaddr,sizeof(clientaddr));
                if(n<0) error("ERROR writing in server2");
                printf("out of order packet:%d received\n",seq);
            }

            if(seq == expected_seqnumber){
                i++;
                expected_seqnumber ++;
                MD5_Update(&mdContext,buf+8,BUFSIZE-8);
                fwrite(buf+8,BUFSIZE-8,1,fd);
                bzero(ack,ACKSIZE);
                createACK(ack,buf);
                n = sendto(sockfd,ack,ACKSIZE,0,(struct sockaddr *)&clientaddr,sizeof(clientaddr));
                if(n<0) error("ERROR writing in server1");
                printf("original Packet:%d received\n",seq);
                printf("i : %d\n", i);
            }

        }
        printf("Received file in %d chunks.\n",no_of_packets );
        
        fclose(fd);
        /*
         *  Compute MD5checksum 
         */
        MD5_Final(checksum,&mdContext);
        checksum[MD5_DIGEST_LENGTH] = '\0';
        printf("%s\n",checksum);
        /* 
         * write: echo the input string back to the client 
         */
        strcpy(buf,checksum);
        //sendReliableUDP(sockfd,buf, clientaddr);
		sendto(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&clientaddr,sizeof(clientaddr));
        
    }
    close(sockfd);
}
