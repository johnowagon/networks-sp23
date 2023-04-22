all : tcp_s help server

tcp_s : tcp_server.c
	gcc -c tcp_server.c -o server.o

help : helpers.c
	gcc -c helpers.c -o helpers.o

server : helpers.o server.o
	gcc server.o -o server
