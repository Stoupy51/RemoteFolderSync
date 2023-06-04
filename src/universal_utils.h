
#ifndef __UNIVERSAL_UTILS_H__
#define __UNIVERSAL_UTILS_H__

#ifdef _WIN32
	#include <windows.h>

	// Sleep function
	#define sleep(seconds) Sleep((int)seconds * 1000)
	typedef SSIZE_T ssize_t;
#else
	#include <unistd.h>
#endif


#endif

