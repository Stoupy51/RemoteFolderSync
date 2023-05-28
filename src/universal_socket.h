
#ifndef __UNIVERSAL_SOCKET_H__
#define __UNIVERSAL_SOCKET_H__

#ifdef _WIN32
	#include <winsock2.h>
	#define socklen_t int
#else
	#include <sys/socket.h>
	#define SOCKET int
#endif


#endif

