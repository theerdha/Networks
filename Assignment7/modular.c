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

#define SSTHRESH 20 * 1024
#define MSS 1024
#define QUEUE_ELEMENTS 100
#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)

/////GLOBAL VARIABLES/////
int SWND = 1;
int retrans = 0; //variable -- 1 : to retransmit 0 : to not
int sender;
int SenderQueueIn, SenderQueueOut;
int ReceiverQueueIn, ReceiverQueueOut;

char* filename;
char buf[MSS]; 
FILE* file;

int client_sockfd, client_portno;
struct sockaddr_in client_serveraddr;
struct hostent *client_server;
char *client_hostname;
int client_serverlen;
MD5_CTX client_mdContext;

int server_clientlen; /* byte size of client's address */
struct sockaddr_in server_serveraddr; /* server's addr */
struct sockaddr_in server_clientaddr; /* client addr */
int server_portno; /* port to listen on */
char * server_hostaddrp; /* dotted decimal host addr string */


char* filename; /* file name*/
char* size_in_string; /* size of file in string*/
char* no_of_packets_str;
int no_of_packets;
int seq;
int size_of_file; /* size of file integer */
char* filedata; /* file data */
MD5_CTX mdContext; /* MD5 context data */
unsigned char checksum[MD5_DIGEST_LENGTH+1]; /* MD5 checksum */
struct stat st;
int length_of_chunk;

void error(char *msg) 
{
    perror(msg);
    exit(0);
}

/////////// STACK IMPLEMENTATION ///////////////
char SenderQueue[QUEUE_ELEMENTS + 1];
char ReceiverQueue[QUEUE_ELEMENTS + 1];

void QueueInit(int a, int b)
{
    a = b = 0;
}

int SenderQueuePut(char new_)
{
    if(SenderQueueIn == (( SenderQueueOut + QUEUE_ELEMENTS) % (QUEUE_ELEMENTS - 1))) return -1; /* Queue Full*/
    SenderQueue[SenderQueueIn] = new_;
    SenderQueueIn = (SenderQueueIn + 1) % QUEUE_SIZE;
    return 0; // No errors
}

int SenderQueueGet(char *old_)
{
    if(SenderQueueIn == SenderQueueOut) return -1; /* Queue Empty - nothing to get*/
    *old = SenderQueue[SenderQueueOut];
    SenderQueueOut = (SenderQueueOut + 1) % QUEUE_SIZE;
    return 0; // No errors
}

int ReceiverQueuePut(char new_)
{
    if(ReceiverQueueIn == (( ReceiverQueueOut + QUEUE_ELEMENTS) % (QUEUE_ELEMENTS - 1))) return -1; /* Queue Full*/
    ReceiverQueue[ReceiverQueueIn] = new_;
    ReceiverQueueIn = (ReceiverQueueIn + 1) % QUEUE_SIZE;
    return 0; // No errors
}

int SenderQueueGet(char *old_)
{
    if(ReceiverQueueIn == ReceiverQueueOut) return -1; /* Queue Empty - nothing to get*/
    *old = ReceiverQueue[ReceiverQueueOut];
    ReceiverQueueOut = (ReceiverQueueOut + 1) % QUEUE_SIZE;
    return 0; // No errors
}

////////////////////MAIN FUNCTION//////////////
int main(int argc, char **argv)
{
	if (argc == 4) {
        sender = 1; // client --> sends the file
        client_hostname = argv[1];
	    client_portno = atoi(argv[2]);
	    filename = argv[3];
	    QueueInit(SenderQueueIn, SenderQueueOut);
	    appSend(client_hostname,client_portno,filename);

    }
    else if (argc == 3) {
        sender = -1; // server --> receives the file
        server_portno = atoi(argv[1]);
        QueueInit(ReceiverQueueIn, ReceiverQueueOut);
        appRecv();
    }

    else
    {
    	fprintf(stderr,"usage: %s <hostname> <port> <filename> or %s <port> <drop-probability>\n", argv[0], argv[0]);
        exit(0);
    }

}

void appSend(char *client_hostname, client_portno,char* filename)
{
	MD5_Init(&mdContext);
    file = fopen(filename,"r");
    //////// TODO : SEND THE FIRST MESSAGE
    char buf;
    bzero(buf,1);

    while(feof(file) == 0)
    {
    	while(SenderQueueIn == (( SenderQueueOut + QUEUE_ELEMENTS) % (QUEUE_ELEMENTS - 1)));
    	fread(buf,1,1,file);
    	MD5_Update(&mdContext,buf,1);
    	SenderQueuePut(char buf);
    	bzero(buf,1);
    }

}

void appRecv()
{
	/////////////TODO: PARSE FIRST PACKET AND GET FILE NAME
	file = fopen(filename,"w+");  
	char buf;      
    bzero(buf,1);
    MD5_Init(&mdContext);
    while(1)
    {
    	while(SenderQueueIn == SenderQueueOut);
    	SenderQueueGet(&buf)
    	/////////TODO END PACKET CHECKING
    	// if( == -100) break; 
    	MD5_Update(&mdContext,buf,1);
        fwrite(buf,1,1,file);
        bzero(buf,1);
    }
    ///////////TODO : send MD5 checksum
}

void * receiver_thread()
{
	char buf[MSS];
	int n;
	int ack;
	char data[MSS];
	int size;
	int RWND;
	while(1)
	{
		bzero(buf,MSS);
		n = recvfrom(sockfd, buf, MSS ,0,(struct sockaddr *)&server_clientaddr,&server_clientlen);
		if (n < 0) error("ERROR in recvfrom1"); 

		//////////TODO PARSE PACKETS
		if(ack == 1)
		{
			update_window(RWND);
		}
		else
		{
			Recv_buffer_handler(data,size);
		}

	}
}




