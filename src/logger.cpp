#include "logger.h"
const char *to_string(severity_level lvl) {
	switch (lvl) {
		case trace:
			return "TRACE";
		case debug:
			return "DEBUG";
		case info:
			return "INFO";
		case warning:
			return "WARNING";
		case error:
			return "ERROR";
		case fatal:
			return "FATAL";
		default:
			return "UNKNOWN";
	}
}

string get_time() {
	time_t time_buffer;
	time(&time_buffer);
	char buf[50] = { 0 };
	string ctime = ctime_r(&time_buffer, buf);
	return ctime.substr(0, ctime.size() - 1);
}
