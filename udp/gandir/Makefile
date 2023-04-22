all : uftp_s uftp_c help server client

uftp_s : uftp_server.c
	gcc -c uftp_server.c -o uftp_server.o

uftp_c : uftp_client.c
	gcc -c uftp_client.c -o uftp_client.o

help : helpers.c
	gcc -c helpers.c -o helpers.o

server : helpers.o uftp_server.o
	gcc uftp_server.o -o server

client : helpers.o uftp_client.o
	gcc uftp_client.o -o client
