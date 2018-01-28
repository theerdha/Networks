binaries: server1 client1
	@echo "Binaries generated in their respective folders."
server1: 
	@gcc server/tcpserver.c -o server/tcpserver -lssl -lcrypto -lm
client1:
	@gcc client/tcpclient.c -o client/tcpclient -lssl -lcrypto
