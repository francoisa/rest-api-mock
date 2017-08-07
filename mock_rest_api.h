#ifndef MOCK_REST_API_H
#define MOCK_REST_API_H

#include <string>
#include <array>

extern const std::string VERSION;

struct Extension {
   const char* ext;
   const std::string filetype;
};

extern const std::array<Extension, 10> extensions;

extern const std::array<std::string, 9> BAD_DIRS;

extern const int ERROR;
extern const int LOG;

void logger(int type, const std::string& s1, const std::string& s2, int
            socket_fd);
void web(int fd, int hit);
#endif
