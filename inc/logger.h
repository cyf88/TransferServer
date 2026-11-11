#ifndef _LOGGER_H
#define _LOGGER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sstream>

enum severity_level { trace, debug, info, warning, error, fatal };

using namespace std;
const char *to_string(severity_level lvl);
string get_time();

// 日志文件流：追加写入当前工作目录下的 TransferServer.log（进程退出时自动关闭）
inline std::ofstream &log_file() {
    static std::ofstream fs("TransferServer.log", std::ios::app);
    return fs;
}

template < typename CharT, typename TraitsT >
inline basic_ostream< CharT, TraitsT > &operator<<(basic_ostream< CharT, TraitsT > &strm, severity_level lvl) {
	strm << get_time() << " " << to_string(lvl);
	return strm;
}

class LogStream {
public:
	LogStream(severity_level lvl, const char* file, int line, const char* func) {
		_buf << lvl << " " << file << " " << line << " " << func << " ";
	}
	~LogStream() {
		_buf << std::endl;
		std::string s = _buf.str();
		std::cout << s;
		std::cout.flush();
		log_file() << s;
		log_file().flush();
	}
	template<typename T>
	LogStream& operator<<(const T& value) {
		_buf << value;
		return *this;
	}

private:
	std::ostringstream _buf;
};

#define BLOG(level) LogStream(level, __FILE__, __LINE__, __func__)

#endif
