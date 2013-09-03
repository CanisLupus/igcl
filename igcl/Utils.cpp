#include "Utils.hpp"

// gets the time between two "gettimeofday" function outputs, in milliseconds
long timeDiff(const timeval & start, const timeval & end) {
	long mtime, seconds, useconds;
	seconds  = end.tv_sec  - start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;
	mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
	return mtime;
}
