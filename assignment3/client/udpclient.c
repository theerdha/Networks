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
#include <sys/stat.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
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

int main(int argc, char **argv) {
    int sockfd, portno, n;
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
    int length_of_chunk;
    int no_of_chunks;

    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
        exit(0);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];

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
    //if (connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
    //    error("ERROR connecting");

    /* get message line from the user */
    // printf("Please enter msg: ");
    // bzero(buf, BUFSIZE);
    // fgets(buf, BUFSIZE, stdin);

    /*
     * Send file name and size of file
     */
    length_of_chunk = 0;
    size_in_string = (char*) malloc(1   
    stat(filename,&st);
    bzero(buf,BUFSIZE);

    inttostr(buf,0,0);
    buf[4] = '\0';
    inttostr(buf,4,0);
    buf[8] = '\0';
    no_of_chunks = st.st_size/BUFSIZE;
    strcpy(buf,filename); 
    sprintf(size_in_string,"%d",st.st_size);
    strcat(buf,":");
    strcat(buf, size_in_string);
    strcat(buf,":");
    length_of_chunk = strlen(buf);
    int temp = strlen(buf);
    inttostr(buf,strlen(buf),no_of_chunks);
    buf[temp+4] = '\0';
    inttostr(buf,4,strlen(buf)-8);

    n = sendto(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
    if (n < 0) 
        error("ERROR writing to socket");
    
    bzero(buf,BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,(socklen_t*)sizeof(serveraddr));
    if (n < 0) 
        error("ERROR reading from socket");
    
    /*
     * Send file 
     */
    MD5_Init(&mdContext);
    file = fopen(filename,"r");
    int i = 0;
    bzero(buf,BUFSIZE);
    while((fread(buf,BUFSIZE,1,file)) > 0){
        n = sendto(sockfd, buf, BUFSIZE,0,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
        MD5_Update(&mdContext,buf,BUFSIZE);
        if (n < 0) 
            error("ERROR writing to socket");
        bzero(buf,BUFSIZE);
        
        n = recvfrom(sockfd, buf, BUFSIZE,0,(struct sockaddr*)&serveraddr,(socklen_t*)sizeof(serveraddr)); 
        if (n < 0) 
            error("ERROR reading from socket");
        bzero(buf,BUFSIZE);
        
        i++;
    }
    printf("File divided into %d chunks for sending.\n",i+1);
        
    MD5_Final(checksum,&mdContext);
    checksum[MD5_DIGEST_LENGTH] = '\0';


    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    if (n < 0) 
        error("ERROR reading from socket");
    //printf("%s, %s\n",buf,checksum);
    if(strcmp(buf,checksum) == 0)
        printf("Check sum matched\n.");
    else
        printf("Check sum not matched\n.");
    close(sockfd);
    return 0;
}
