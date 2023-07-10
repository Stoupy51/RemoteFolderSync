
#ifndef __UNIVERSAL_SOCKET_H__
#define __UNIVERSAL_SOCKET_H__

#ifdef _WIN32
	#include <winsock2.h>
	#define socklen_t int

	#define socket_close(socket) closesocket(socket)
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#define SOCKET int
	#define INVALID_SOCKET -1

	#define socket_close(socket) close(socket)
#endif
#define socket_read(socket, buffer, size, flags) recv(socket, (char*)buffer, size, flags)
#define socket_write(socket, buffer, size, flags) send(socket, (char*)buffer, size, flags)

#endif

