/* 
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <openssl/md5.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 4	) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd, &serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");

    /* get message line from the user */

	unsigned char c[MD5_DIGEST_LENGTH];
	FILE *inFile = fopen (argv[3], "rb");
	MD5_CTX mdContext;
    int bytes,i;
    unsigned char data[1024];
	MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);

	fseek( inFile , 0L , SEEK_END);
	long lSize = ftell( inFile );
	rewind( inFile );
	char * buffer = calloc( 1, lSize+1 );
	if( 1!=fread( buffer , lSize, 1 , inFile) )
    fclose(inFile),free(buffer),fputs("entire read fails",stderr),exit(1);

    /* send the message line to the server */
    n = write(sockfd, buffer, lSize);
    if (n < 0) 
      error("ERROR writing to socket");

	fclose(inFile);
	free(buffer);

    
    
    if (n < 0) 
      error("ERROR reading from socket");

	for(i = 0; i < MD5_DIGEST_LENGTH; i++)printf("%02x",c[i]);
	printf("\n");
    
    char mdString[2*MD5_DIGEST_LENGTH];
    for(i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)c[i]);

    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    //printf("%c\n",buf[0]);
    
	for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	{
		if(mdString[i] != buf[i])
		{
			printf("MD5 not matched\n");
            printf("received %c, actual %c,index %d\n",buf[i],mdString[i],i);
			close(sockfd);
    		return 0;
		}
	}
	printf("MD5 matched\n");
    close(sockfd);
    return 0;
}
