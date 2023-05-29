
#ifndef __UNIVERSAL_SOCKET_H__
#define __UNIVERSAL_SOCKET_H__

#ifdef _WIN32
	#include <winsock2.h>
	#define socklen_t int

	#define socket_read(socket, buffer, size) recv(socket, (char*)buffer, size, 0)
	#define socket_write(socket, buffer, size) send(socket, (char*)buffer, size, 0)
	#define socket_close(socket) closesocket(socket)
#else
	#include <sys/socket.h>
	#define SOCKET int
	#define INVALID_SOCKET -1

	#define socket_read(socket, buffer, size) read(socket, (char*)buffer, size)
	#define socket_write(socket, buffer, size) write(socket, (char*)buffer, size)
	#define socket_close(socket) close(socket)
#endif


#endif

