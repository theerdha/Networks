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
#include <math.h>

#define BUFSIZE 1024
#define ACKSIZE 64
#define TIMEOUT 1
#define MAX_WINDOW_SIZE 1000
/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

#include "../udpreliable.h"

typedef struct CHUNK
{
    int chunk_no;
    char buf[BUFSIZE];
}chunk;

void copy_struct(chunk* a,chunk* b)
{
    a -> chunk_no = b -> chunk_no;
    int i;
    for(i = 0; i < BUFSIZE; i++)
    {
        a -> buf[i] = b -> buf[i];
    }
}

chunk chunk_buffer[MAX_WINDOW_SIZE];
int ind = -1;
int seq = 0;
int sockfd, portno;
struct sockaddr_in serveraddr;
struct hostent *server;
char *hostname;
int serverlen;
MD5_CTX mdContext;
int no_of_chunks;
unsigned char checksum[MD5_DIGEST_LENGTH+1];
int WINDOW_SIZE = 3;

void remove_from_buffer(int n)
{
    int i;
    for(i = 0; i < n; i ++)
    {
        if(n + i <= ind)copy_struct(&chunk_buffer[i],&chunk_buffer[n + i]);
    }
    ind = ind - n;
}

void add_to_buffer(int seq, char buf[],int chunk_buffer_count[])
{
    ind ++;
    chunk_buffer[ind].chunk_no = seq;
    int i;
    for(i = 0; i < BUFSIZE; i++)
    {
        chunk_buffer[ind].buf[i] = buf[i];

    }
    chunk_buffer_count[seq] = 1;   
}

int buffer_index(int seq)
{
    int i;
    for(i = 0; i < ind; i ++)
    {
        if(chunk_buffer[i].chunk_no == seq)
        {
           return i; 
        } 
    }
    return -1;
}

void sender_func(int start, int end, FILE* file,int chunk_buffer_count[])
{
    int i = start;
    char buf[BUFSIZE];
    bzero(buf,BUFSIZE);
    if(end > no_of_chunks) end = no_of_chunks;
     
    while(i <= end)
    {
        
        if(chunk_buffer_count[i] == -1)
        {
            fread(buf+8,BUFSIZE-8,1,file);
            MD5_Update(&mdContext,buf+8,BUFSIZE-8);
            setSequenceNumber(buf,&seq);
            setMessageSize(buf,strlen(buf));
            sendto(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
            add_to_buffer(strtoint(buf,0),buf,chunk_buffer_count);
            i ++;
            WINDOW_SIZE *= 2;
        }
        else
        {
            sendto(sockfd, chunk_buffer[buffer_index(i)].buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
            WINDOW_SIZE /= 2;
        }

    }
}

int main(int argc, char **argv) {
    int n;
    char* filename;
    char buf[BUFSIZE]; 
    FILE* file;
    char temp;
    char* size_in_string;
    char* no_of_chunks_str;
    char* filename_size;
    struct stat st;
    int length_of_chunk;
    struct timeval timeout;
	int startptr = 1;
	int endptr = 3;
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
    no_of_chunks = ceil(st.st_size/(double)(BUFSIZE-8));
    int chunk_buffer_count[no_of_chunks + 1];
    int i = 0;

    for(i = 0; i <= no_of_chunks; i++)
    {
        chunk_buffer_count[i] = -1;
    }

    
    bzero(buf,BUFSIZE);
    //buf[8] = '\0';
    strcpy(buf+8,filename); 
    sprintf(size_in_string,"%d",(int)st.st_size);
    strcat(buf+8,":");
    strcat(buf+8, size_in_string);
    strcat(buf+8,":");
    sprintf(no_of_chunks_str,"%d",no_of_chunks);
    strcat(buf+8, no_of_chunks_str);
    setSequenceNumber(buf,&seq);
    setMessageSize(buf,strlen(buf));
    //sendReliableUDP(sockfd,buf,serveraddr); 
   	n = sendto(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
    if (n < 0) error("ERROR in sendto");
 
    MD5_Init(&mdContext);
    file = fopen(filename,"r");
    i = 0;
    bzero(buf,BUFSIZE);
	
    serverlen = sizeof(serveraddr);
    printf("File will be sent in %d packets.\n",no_of_chunks);
    while(1){

        if(endptr > no_of_chunks) endptr = no_of_chunks;
        //printf("send called from %d and %d\n",startptr,endptr);
        if(startptr >= endptr)break;
		sender_func(startptr,endptr,file,chunk_buffer_count);
		int count;
        int max = startptr - 1;
        for(count = 0; count < endptr - startptr + 1; count++)
        {
            bzero(buf, BUFSIZE);
            n = recvfrom(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,&serverlen);
            if (n < 0) error("ERROR in recvfrom1"); 
            int s = strtoint(buf,0);
            printf("ack received for %d seq num\n",s);
            if(s > max) max = s;
            if(max == no_of_chunks)break;
        }
        //printf("here %d\n", max); 

        remove_from_buffer(max - startptr + 1);
        
        int x;
        for(x = startptr; x <= max; x++)
        {
            chunk_buffer_count[x] = -1;
        } 
        startptr = max + 1;
        endptr = startptr + WINDOW_SIZE -1;
		
    }
    printf("\nFile sent in %d windows.\n",i);
     
    MD5_Final(checksum,&mdContext);
    checksum[MD5_DIGEST_LENGTH] = '\0';


    /* print the server's reply */
    bzero(buf, BUFSIZE);
    //recvReliableUDP(sockfd,buf,&serveraddr);
	recvfrom(sockfd, buf, BUFSIZE,0,(struct sockaddr *)&serveraddr,(socklen_t*)sizeof(serveraddr));
    printf("%s, %s\n",buf,checksum);
    if(strcmp(buf,checksum) == 0)
        printf("Check sum matched\n.");
    else
        printf("Check sum not matched\n.");
    close(sockfd);
    return 0;
}
