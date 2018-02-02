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
#define TIMEOUT 5
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
    int serverlength = sizeof(serveraddr);
    while(1){
        n = sendto(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
        if (n < 0) 
            error("ERROR writing to socket");
        bzero(recvbuf,BUFSIZE);
        n = recvfrom(sockfd, recvbuf, BUFSIZE,0,(struct sockaddr *)&serveraddr,&serverlength);
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
    return;
}

void setSequenceNumber(char* buf,int* index){
    inttostr(buf,0,*index);
    *index = *index+1;
}

void setMessageSize(char* buf,int size){
    inttostr(buf,4,size);
}



int main(int argc, char **argv) {
    int sockfd, portno, n,seq = 0;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    int serverlen;
    char* filename;
    char buf[BUFSIZE];
    FILE* file;
    char temp;
    char* size_in_string;
    char* no_of_chunks_str;
    MD5_CTX mdContext;
    unsigned char checksum[MD5_DIGEST_LENGTH+1];
    char* filename_size;
    struct stat st;
    int length_of_chunk;
    int no_of_chunks;
    struct timeval timeout;
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
    
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(const char*) &timeout,sizeof(timeout));

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
    size_in_string = (char*) malloc(10);
    no_of_chunks_str = (char*) malloc(10);
    
    stat(filename,&st);
    no_of_chunks = st.st_size/(BUFSIZE-8);
    
    bzero(buf,BUFSIZE);
    strcpy(buf+8,filename); 
    sprintf(size_in_string,"%d",st.st_size);
    strcat(buf,":");
    strcat(buf, size_in_string);
    strcat(buf,":");
    sprintf(no_of_chunks_str,"%d",no_of_chunks);
    strcat(buf, no_of_chunks_str);

    setSequenceNumber(buf,&seq);
    setMessageSize(buf,strlen(buf));
    sendReliableUDP(sockfd,buf,serveraddr); 
    
    MD5_Init(&mdContext);
    file = fopen(filename,"r");
    int i = 0;
    bzero(buf,BUFSIZE);
    
    while(feof(file) == 0){
        fread(buf+8,BUFSIZE-8,1,file);

        MD5_Update(&mdContext,buf,BUFSIZE);
        
        setSequenceNumber(buf,&seq);
        setMessageSize(buf,strlen(buf));
        sendReliableUDP(sockfd,buf,serveraddr);
        
        bzero(buf,BUFSIZE);
        i++;
    }
    printf("File divided into %d chunks for sending.\n",i+1);
        
    MD5_Final(checksum,&mdContext);
    checksum[MD5_DIGEST_LENGTH] = '\0';


    /* print the server's reply */
    bzero(buf, BUFSIZE);
    recvReliableUDP(sockfd,buf,serveraddr);

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
