#ifndef FS_HPP
#define FS_HPP

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <string>


class AbstractFile {
public:
	virtual ssize_t read(char *buf, size_t len) = 0;
	virtual ssize_t write(char *buf, size_t len) = 0;
	virtual ssize_t seek(size_t offset, int method) = 0;
	virtual ~AbstractFile() {}
};

class HostFile: public AbstractFile {
private:
	int hostFd;
	std::string path;
};
#endif
