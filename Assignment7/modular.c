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
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>


#define MSS 1024
#define SENDER_QUEUE_ELEMENTS 100000
#define SENDER_QUEUE_SIZE (SENDER_QUEUE_ELEMENTS + 1)
#define RECEIVER_QUEUE_ELEMENTS 100000
#define RECEIVER_QUEUE_SIZE (RECEIVER_QUEUE_ELEMENTS + 1)
#define SLEEP_VAL 1
#define TIMEOUT 1

/////GLOBAL VARIABLES/////
int SSTHRESH = 20 * 1024;
int SWND = 1;
int CWND = 1;
int retrans = 0; //variable -- 1 : to retransmit 0 : to not
int sender;
int SenderQueueIn, SenderQueueOut;
int ReceiverQueueIn, ReceiverQueueOut;
int base = 1;
int nextseqnum = 1;
int ackrecv = 0;
int timeroff = 0;
int expectedseqnum = 1;
int last_ack_length;
int senderseqno = 1;
int expected_seqnumber = 1;
struct timeval timeout;

char* filename;
char buf[MSS]; 
FILE* file;

int client_sockfd, client_portno;
struct sockaddr_in client_serveraddr;
struct hostent *client_server;
char *client_hostname;
int client_serverlen;
MD5_CTX client_mdContext;

int server_sockfd;
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

typedef struct DATA_PACKET
{
	int ack;
	int length;
	int seqnum;
	int rwnd;
	char payload[MSS - 16];
}data_packet;

void update_window(int RWND,int seq, int size);
void Recv_buffer_handler(data_packet d);

data_packet temp_buff[100];
int ind = -1;

static int alarm_status = 0;
void mysig(int sig){
    pid_t pid;
    printf("PARENT : Received signal %d \n", sig);
    if (sig == SIGALRM){
        alarm_status = 1;
    }
}


void copy_packet(data_packet* a, data_packet * b)
{
	a -> ack = b -> ack;
    a -> length = b -> length;
    a -> seqnum = b -> seqnum;
    a -> rwnd = b -> rwnd;

    int i;
    for(i = 0; i < a -> length; i++)
    {
        a -> payload[i] = b -> payload[i];
    }
}

void remove_from_buffer(int n)
{
    int i;
    for(i = 0; i < n; i ++)
    {
        if(n + i <= ind)copy_packet(&temp_buff[i],&temp_buff[n + i]);
    }
    ind = ind - n;
}

void add_to_buffer(data_packet d)
{
    ind ++;
    temp_buff[ind].seqnum = d.seqnum;
    temp_buff[ind].length = d.length;
    temp_buff[ind].ack = d.ack;
    temp_buff[ind].rwnd = d.rwnd;

    int i;
    for(i = 0; i < d.length; i++)
    {
        temp_buff[ind].payload[i] = d.payload[i];

    }
    //seq_in_buffer[d.seqnum] = 1;   
}

int buffer_index(int seq)
{
    int i;
    for(i = 0; i < ind; i ++)
    {
        if(temp_buff[i].seqnum == seq)
        {
           return i; 
        } 
    }
    return -1;
}

void error(char *msg) 
{
    perror(msg);
    exit(0);
}

/////////// STACK IMPLEMENTATION ///////////////
char SenderQueue[SENDER_QUEUE_ELEMENTS + 1];
char ReceiverQueue[RECEIVER_QUEUE_ELEMENTS + 1];

void QueueInit(int a, int b)
{
    a = b = 0;
}

int SenderQueuePut(char new_)
{
    if(SenderQueueIn == (( SenderQueueOut + SENDER_QUEUE_SIZE - 1) % (SENDER_QUEUE_SIZE))) return -1; /* Queue Full*/
    SenderQueue[SenderQueueIn] = new_;
    SenderQueueIn = (SenderQueueIn + 1) % SENDER_QUEUE_SIZE;
    return 0; // No errors
}

int SenderQueueGet(char *old_)
{
    if(SenderQueueIn == SenderQueueOut) return -1; /* Queue Empty - nothing to get*/
    *old_ = SenderQueue[SenderQueueOut];
    SenderQueueOut = (SenderQueueOut + 1) % SENDER_QUEUE_SIZE;
    return 0; // No errors
}

int ReceiverQueuePut(char new_)
{
    if(ReceiverQueueIn == (( ReceiverQueueOut + RECEIVER_QUEUE_SIZE - 1) % (RECEIVER_QUEUE_SIZE))) return -1; /* Queue Full*/
    ReceiverQueue[ReceiverQueueIn] = new_ ;
    ReceiverQueueIn = (ReceiverQueueIn + 1) % RECEIVER_QUEUE_SIZE;
    return 0; // No errors
}

int ReceiverQueueGet(char *old_)
{
    if(ReceiverQueueIn == ReceiverQueueOut) return -1; /* Queue Empty - nothing to get*/
    *old_  = ReceiverQueue[ReceiverQueueOut];
    ReceiverQueueOut = (ReceiverQueueOut + 1) % RECEIVER_QUEUE_SIZE;
    return 0; // No errors
}


void appSend(char *client_hostname, int client_portno,char* filename)
{
	MD5_Init(&mdContext);
    file = fopen(filename,"r");
    struct stat st;
    stat(filename,&st);
    size_in_string = (char*) malloc(10);
    sprintf(size_in_string,"%d",(int)st.st_size);

    ///SEND THE FIRST MESSAGE

    data_packet first;
    first.seqnum = 0;
    first.ack = 0;
    strcpy(first.payload,filename);
    strcat(first.payload,":");
    strcat(first.payload,size_in_string);
    first.length = strlen(first.payload);

    data_packet * x = (data_packet *)malloc(sizeof(first));
    char charbuff[MSS];
    *x = first;
    x = (data_packet *)charbuff;
    *x = first;
    printf("appsend....\n");
    int n = sendto(client_sockfd, charbuff, sizeof(first),0,(struct sockaddr *)&client_serveraddr,sizeof(client_serveraddr));
    if (n < 0) error("ERROR in sendto");

    char buf;
    bzero(&buf,1);

    while(feof(file) == 0)
    {
    	while(SenderQueueIn == (( SenderQueueOut + SENDER_QUEUE_ELEMENTS) % (SENDER_QUEUE_ELEMENTS - 1)));
    	fread(&buf,1,1,file);
    	MD5_Update(&mdContext,&buf,1);
    	SenderQueuePut(buf);
    	bzero(&buf,1);
    }
    
	MD5_Final(checksum,&mdContext);
    checksum[MD5_DIGEST_LENGTH] = '\0';
    bzero(charbuff, MSS);
	recvfrom(client_sockfd, charbuff, MSS,0,(struct sockaddr *)&client_serveraddr,(socklen_t*)sizeof(client_serveraddr));
    printf("%s, %s\n",charbuff,checksum);
    if(strcmp(charbuff,checksum) == 0)
        printf("Check sum matched\n.");
    else
        printf("Check sum not matched\n.");

}

void appRecv()
{
	/////PARSE FIRST PACKET AND GET FILE NAME
	char mess[MSS];
	bzero(mess, MSS);
	int n = recvfrom(server_sockfd, mess, MSS,0,(struct sockaddr *)&server_clientaddr,&server_clientlen);
    if (n < 0) error("ERROR in recvfrom2"); 

    ///typecast from char to data_packet.
    data_packet * a =(data_packet *)malloc(sizeof(mess));
	data_packet b;
	a = (data_packet *)mess;
	b = *a;
	
	filename = strtok(b.payload,":");
	size_in_string = strtok(NULL,":");
    size_of_file = atoi(size_in_string);
        
	file = fopen(filename,"w+");  
	char buf;      
    MD5_Init(&mdContext);
    int count = 0;
    while(count < size_of_file)
    {
    	while(ReceiverQueueIn == ReceiverQueueOut);
    	ReceiverQueueGet(&buf);
    	MD5_Update(&mdContext,&buf,1);
	    fwrite(&buf,1,1,file);
    	count ++;
    }
    //send MD5 checksum
    MD5_Final(checksum,&mdContext);
    checksum[MD5_DIGEST_LENGTH] = '\0';
    printf("%s\n",checksum);
    strcpy(mess,checksum);
	n = sendto(server_sockfd, mess, MSS,0,(struct sockaddr *)&server_clientaddr,sizeof(server_clientaddr));
	if (n < 0) error("ERROR in sendto");
	fclose(file);
}

void * receiver_thread(void* unsused)
{
	char buf[MSS];
	int n;
	while(1)
	{
		bzero(buf,MSS);
		n = recvfrom(server_sockfd, buf, MSS ,0,(struct sockaddr *)&server_clientaddr,&server_clientlen);
		if (n < 0) error("ERROR in recvfrom1"); 

		data_packet * a= (data_packet *)malloc(sizeof(buf));
		data_packet b;
		a = (data_packet *)buf;
		b = *a;

		//PARSE PACKETS
		if(b.ack == 1)
		{
			ackrecv = 1;
			base = b.seqnum + b.length;
	    	if(base == nextseqnum) alarm_status = 1; 
	    	else alarm(SLEEP_VAL);
	    	////////remove_from_buffer(1);
			update_window(b.rwnd,b.seqnum,b.length);
		}
		else
		{
			Recv_buffer_handler(b);
		}

	}
	pthread_exit(0);
}

data_packet create_packet(int aa)
{
	printf("creating packet\n");
	data_packet new_;
	new_.ack = 0;
	new_.seqnum = aa;

	if(base + SWND - aa >= MSS - 16) new_.length = MSS - 16;	
	else new_.length = base + SWND - aa;
		
	for(int x = 0; x < new_.length; x++)
	{
		SenderQueueGet(&new_.payload[x + aa]);
	}	
	return new_;
}

void retransmit()
{
	data_packet * x;
    char charbuff[MSS];
    int i,n;
    for(i = 0; i < ind; i++)
    {
    	if(temp_buff[i].seqnum + temp_buff[i].length <= base + SWND)
	    {
	    	*x = temp_buff[i];
	    	 x = (data_packet *)charbuff;
	    	*x = temp_buff[i];
	    	n = sendto(client_sockfd, charbuff, sizeof(temp_buff[i]),0,(struct sockaddr *)&client_serveraddr,sizeof(client_serveraddr));
	    	if (n < 0) error("ERROR in sendto");
	    	ackrecv = 0;
	    }
	    else break;
    }

    if(base + SWND < nextseqnum && i != ind + 1)  
    {
    	data_packet d = create_packet(temp_buff[i - 1].seqnum);
    	*x = d;
	    x = (data_packet *)charbuff;
	    *x = d;
	    n = sendto(client_sockfd, charbuff, d.length + 16,0,(struct sockaddr *)&client_serveraddr,sizeof(client_serveraddr));
	    if (n < 0) error("ERROR in sendto");
	    ackrecv = 0;
    }

    while(nextseqnum <= base + SWND)
    {
    	data_packet d = create_packet(nextseqnum);
    	nextseqnum += d.length;
    	*x = d;
	    x = (data_packet *)charbuff;
	    *x = d;

	    n = sendto(client_sockfd, charbuff, d.length + 16,0,(struct sockaddr *)&client_serveraddr,sizeof(client_serveraddr));
	    if (n < 0) error("ERROR in sendto");
		add_to_buffer(d);
		ackrecv = 0;
    }  

}


void * rate_control(void* unsused)
{	

	while(size_of_file --)
	{	
		printf("in rate control\n");
		if(alarm_status == 0)
		{
			if(nextseqnum < base + SWND && nextseqnum < SenderQueueIn)
			{
				//createpacket function
				if(base == nextseqnum)
	            alarm(SLEEP_VAL);

				data_packet d = create_packet(nextseqnum);
				data_packet * x;
			    char charbuff[MSS];
			    *x = d;
			    x = (data_packet *)charbuff;
			    *x = d;

			    int n = sendto(client_sockfd, charbuff, d.length + 16,0,(struct sockaddr *)&client_serveraddr,sizeof(client_serveraddr));
			    if (n < 0) error("ERROR in sendto");
				add_to_buffer(d);//add the data packet to buffer
				nextseqnum += d.length;
				ackrecv = 0;
			}
		}
		else
		{
			//// retransmission
			retrans = 1;
			update_window(0,0,0);
			alarm(SLEEP_VAL);
			retransmit();
			retrans = 0;

		}	
	}
	pthread_exit(0);

}

int min(int a, int b)
{
	if(a < b)return a;
	else return b;
}

/////should also be called when retransmission happens.
void update_window(int RWND,int seq, int size)
{
	static int dupack = 0;

	if(retrans == 1)
	{
		SSTHRESH /= 2;
		CWND = 1;
	}

	else
	{	
		if(seq == base - 1)
		{
			dupack ++;
			if(dupack == 3)
			{
				SSTHRESH = SSTHRESH / 2;
				CWND = SSTHRESH;
			}
		}

		if(seq >= base)
		{
			if(CWND < SSTHRESH) CWND += MSS;
			else CWND += (MSS / CWND);
			dupack = 0;
		}
	}
	SWND = min(RWND,CWND);
}

void Recv_buffer_handler(data_packet d)
{

	if(d.seqnum != expected_seqnumber)
    {
    	// create a duplicate acknowledgement and send
    	data_packet ac;
    	data_packet * acptr;
    	char buff[MSS];
    	ac.ack = 1;
    	ac.seqnum = expectedseqnum - last_ack_length;
    	ac.length = last_ack_length;
    	ac.rwnd = RECEIVER_QUEUE_SIZE - ReceiverQueueIn; //////modify this!!!!
    	acptr = (data_packet *)buff;
    	*acptr = ac;
    	int n = sendto(server_sockfd,buff,sizeof(ac),0,(struct sockaddr *)&server_clientaddr,sizeof(server_clientaddr));
        if(n<0) error("ERROR writing in server3");
        printf("duplicate packet:%d received\n",d.seqnum);
    }


    if(d.seqnum == expected_seqnumber)
    {
    	
    	for(int x = 0; x < d.length; x++)
    	{
    		expectedseqnum ++;
    		ReceiverQueuePut(d.payload[x]);
    	}

    	last_ack_length = d.length;
    	
    	// create an ack and send it
    	data_packet ac;
    	data_packet * acptr;
    	char buff[MSS];
    	ac.ack = 1;
    	ac.seqnum = d.seqnum;
    	ac.length = d.length;
    	ac.rwnd = RECEIVER_QUEUE_SIZE - ReceiverQueueIn; //////modify this!!!!
    	acptr = (data_packet *)buff;
    	*acptr = ac;
    	int n = sendto(server_sockfd,buff,sizeof(ac),0,(struct sockaddr *)&server_clientaddr,sizeof(server_clientaddr));
        if(n<0) error("ERROR writing in server3");
        printf("original Packet:%d received\n",d.seqnum);
    }
}

////////////////////MAIN FUNCTION//////////////
int main(int argc, char **argv)
{

	(void) signal(SIGALRM,mysig);
	if (argc != 4 && argc != 2)
	{
		fprintf(stderr,"usage: %s <hostname> <port> <filename> or %s <port> <drop-probability>\n", argv[0], argv[0]);
        exit(0);	
	}
	pthread_t tid1,tid2;
	pthread_attr_t attr1,attr2;
	pthread_attr_init(&attr1);
	pthread_attr_init(&attr2);

	if (argc == 4) {
        sender = 1; // client --> sends the file
        client_hostname = argv[1];
	    client_portno = atoi(argv[2]);
	    filename = argv[3];

	    QueueInit(SenderQueueIn, SenderQueueOut);
	    client_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	    if (client_sockfd < 0) 
	        error("ERROR opening socket");
	    /* gethostbyname: get the server's DNS entry */
	    client_server = gethostbyname(client_hostname);
	    if (client_server == NULL) {
	        fprintf(stderr,"ERROR, no such host as %s\n", client_hostname);
	        exit(0);
	    }
	    
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	    //setsockopt(client_sockfd,SOL_SOCKET,SO_RCVTIMEO,(const char*) &timeout,sizeof(timeout));

	    /* build the server's Internet address */
	    bzero((char *) &client_serveraddr, sizeof(client_serveraddr));
	    client_serveraddr.sin_family = AF_INET;
	    bcopy((char *)client_server->h_addr, 
	            (char *)&client_serveraddr.sin_addr.s_addr, client_server->h_length);
	    client_serveraddr.sin_port = htons(client_portno);

	    pthread_create(&tid1,&attr1,rate_control,NULL);
		pthread_create(&tid2,&attr2,receiver_thread,NULL);
		printf("client Running ....\n");
	    appSend(client_hostname,client_portno,filename);
	    
		
    }
    else if (argc == 2) {
        sender = -1; // server --> receives the file
        server_portno = atoi(argv[1]);
        QueueInit(ReceiverQueueIn, ReceiverQueueOut);
        server_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	    if (server_sockfd < 0) error("ERROR opening socket");
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	    setsockopt(server_sockfd, SOL_SOCKET, SO_RCVTIMEO|SO_REUSEADDR, (const char *)&timeout , sizeof(timeout));

	    bzero((char *) &server_serveraddr, sizeof(server_serveraddr));
	    server_serveraddr.sin_family = AF_INET;
	    server_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	    server_serveraddr.sin_port = htons((unsigned short)server_portno);

	    if (bind(server_sockfd, (struct sockaddr *) &server_serveraddr, sizeof(server_serveraddr)) < 0) error("ERROR on binding");
	    printf("Server Running ....\n");
	    server_clientlen = sizeof(server_clientaddr);
        pthread_create(&tid1,&attr1,rate_control,NULL);
		pthread_create(&tid2,&attr2,receiver_thread,NULL);
		appRecv();
    }

    pthread_join(tid1,NULL);
    pthread_join(tid2,NULL);
    return 0;

}