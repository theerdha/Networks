/* 
 * udpserver.c - A UDP echo server 
 * usage: udpserver <port_for_server>
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

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
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
    fprintf(stderr, "usage: %s <port_for_server>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the socket 
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
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

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
    for(int i = 0; i < chunks;i++){
      n = recvfrom(sockfd, buf, BUFSIZE, 0,(struct sockaddr *) &clientaddr, &clientlen);
      MD5_Update(&mdContext,buf,BUFSIZE);
      if (n < 0) 
          error("ERROR reading from socket");
      if(n == 0)
          error("No file chunk received");
      //if(n > 0)
      //    printf("File chunk %d recieved.\n",i+1);
      fwrite(buf,n,1,fd);
      bzero(buf,BUFSIZE);
    }



    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);

    printf("Received file in %d chunks.\n",chunks );
        
    fclose(fd);
    /*
     *  Compute MD5checksum 
     */
    MD5_Final(checksum,&mdContext);
    checksum[MD5_DIGEST_LENGTH] = '\0';

    /* 
     * sendto: echo the input back to the client 
     */
    n = sendto(sockfd,checksum, MD5_DIGEST_LENGTH+1, 0, 
	       (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
  }
}
