/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
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
#include <sys/stat.h>

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
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char* filename;
    char buf[BUFSIZE];
    FILE* file;
    char temp;
    char* size_in_string;
    MD5_CTX mdContext;
    unsigned char checksum[MD5_DIGEST_LENGTH+1];
    char* filename_size;
    struct stat st;

    /* check command line arguments */
    if (argc != 4) {
       fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    /* get a message from the user */
    /*bzero(buf, BUFSIZE);
    printf("Please enter msg: ");
    fgets(buf, BUFSIZE, stdin);*/

    /* send the message to the server */
    serverlen = sizeof(serveraddr);

    stat(filename,&st);
    bzero(buf,BUFSIZE);
    strcpy(buf,filename);
    size_in_string = (char*) malloc(10);
    sprintf(size_in_string,"%d",(int)st.st_size);
    strcat(buf,":");
    strcat(buf, size_in_string);

    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");

    MD5_Init(&mdContext);
    file = fopen(filename,"r");
    int i = 0;
    bzero(buf,BUFSIZE);
    while(feof(file) == 0){
        fread(buf,BUFSIZE,1,file);
        n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
        MD5_Update(&mdContext,buf,BUFSIZE);
        if (n < 0) 
            error("ERROR writing to socket");
        bzero(buf,BUFSIZE);
        i++;
    }
    printf("File divided into %d chunks for sending.\n",i);
        
    MD5_Final(checksum,&mdContext);
    checksum[MD5_DIGEST_LENGTH] = '\0';

    
    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
    if(strcmp(buf,checksum) == 0)
        printf("Check sum matched\n.");
    else
        printf("Check sum not matched\n.");
    return 0;
}
