
#ifndef __UNIVERSAL_SOCKET_H__
#define __UNIVERSAL_SOCKET_H__

#ifdef _WIN32
	#include <winsock2.h>
	typedef int socklen_t;
	typedef SOCKET socket_t;

	#define socket_close(socket) closesocket(socket)
#else
	#include <unistd.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	typedef int socket_t;
	#define INVALID_SOCKET -1

	#define socket_close(socket) close(socket)
#endif

#define tcp_read(socket, buffer, size, flags) recv(socket, (char*)buffer, size, flags)
#define tcp_write(socket, buffer, size, flags) send(socket, (char*)buffer, size, flags)

#endif

