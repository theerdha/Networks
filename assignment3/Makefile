binaries: tcp 
	@echo "Binaries generated in their respective folders."
tcp: tcpserver tcpclient

udp: udpserver udpclient

tcpserver: 
	@gcc server/tcpserver.c -o server/tcpserver -lssl -lcrypto -lm
tcpclient:
	@gcc client/tcpclient.c -o client/tcpclient -lssl -lcrypto
udpserver:
	@gcc server/udpserver.c -o server/udpserver -lssl -lcrypto -lm
udpclient:
	@gcc client/udpclient.c -o client/udpclient -lssl -lcrypto 
