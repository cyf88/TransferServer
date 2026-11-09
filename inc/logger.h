#ifndef _LOGGER_H
#define _LOGGER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

enum severity_level { trace, debug, info, warning, error, fatal };

using namespace std;
const char *to_string(severity_level lvl);
string get_time();
template < typename CharT, typename TraitsT >
inline basic_ostream< CharT, TraitsT > &operator<<(basic_ostream< CharT, TraitsT > &strm, severity_level lvl) {
	strm << get_time() << " " << to_string(lvl);
	return strm;
}

class LogStream {
public:
	LogStream(severity_level lvl, const char* file, int line, const char* func) {
		cout << lvl << " " << file << " " << line << " " << func << " ";
	}
	~LogStream() {
		cout << std::endl;
	}
	template<typename T>
	LogStream& operator<<(const T& value) {
		cout << value;
		return *this;
	}
};

#define BLOG(level) LogStream(level, __FILE__, __LINE__, __func__)

#endif