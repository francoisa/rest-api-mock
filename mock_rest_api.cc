#include "mock_rest_api.h"
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <map>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>

const std::string VERSION = "1.0";
const int ERROR     =   42;
const int LOG       =   44;

static const int BUFSIZE   = 8096;
static const int HEADER    =   45;
static const int FORBIDDEN =  403;
static const int NOTFOUND  =  404;

const std::array<Extension, 10> extensions = { {
   {"gif", "image/gif" },
   {"jpg", "image/jpg" },
   {"jpeg", "image/jpeg"},
   {"png", "image/png" },
   {"ico", "image/ico" },
   {"zip", "image/zip" },
   {"gz", "image/gz"  },
   {"tar", "image/tar" },
   {"htm", "text/html" },
   {"html", "text/html" } } };

const std::array<std::string, 9> BAD_DIRS = {
   { "/", "/bin", "/etc", "lib", "/tmp", "/usr", "/var", "/opt", "/proc" } };

void *ThreadMain(void *arg); // Main program of a thread

// Structure of arguments to pass to client thread
struct ThreadArgs {
    int clntSock; // Socket descriptor for client
};

std::map<std::string, std::string> rest_data;

void logger(int type, const std::string& s1, const std::string& s2, int
socket_fd) {
   std::ofstream log;
   log.open("mock_rest_api.log", std::ofstream::app);
   std::ostringstream resp;

   switch (type) {
   case ERROR: log << "ERROR: " << s1 << ": " << s2 << " Errno = "
                   << errno << " exiting pid =" << getpid() << std::endl;
     break;
   case FORBIDDEN:
     resp << "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\n"
          << "Content-Type: text/html\n\n<html><head>\n"
          << "<title>403 Forbidden</title>\n</head><body>\n"
          << "<h1>Forbidden</h1>\n"
          << "The requested URL, file type or operation is not allowed on "
          << "this simple static file webserver.\n</body></html>\n";
     write(socket_fd, resp.str().c_str(), resp.str().size());
     log << "FORBIDDEN: " << s1 << ": " << s2 << std::endl;
     break;
   case NOTFOUND:
     resp << "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\n"
          << "Content-Type: text/html\n\n<html><head>\n"
          << "<title>404 Not Found</title>\n</head><body>\n"
          << "<h1>Not Found</h1>\nThe requested URL was not found on this"
          << " server.\n</body></html>\n";
     write(socket_fd, resp.str().c_str(), resp.str().size());
     log << "NOT FOUND: " << s1 << ": " << s2 << std::endl;
     break;
   case LOG:
     log << "INFO: " << s1 << ": " << s2 << " Socket ID:" << socket_fd << std::endl;
     break;
   case HEADER:
     log << s1 << ":\n" << s2 << "ID: " << socket_fd << std::endl;
   }
   log.close();
   /* No checks here, nothing can be done with a failure anyway */
   if (type == ERROR || type == NOTFOUND || type == FORBIDDEN) {
     exit(3);
   }
}

enum class Method {GET, POST};

Method parse_method(const char* buffer, int fd) {
   static const std::string GET{"GET "};
   static const std::string get{"get "};
   static const std::string POST{"POST "};
   static const std::string post{"post "};
   if (strncmp(buffer, GET.c_str(), GET.size()) == 0 ||
       strncmp(buffer, get.c_str(), get.size()) == 0) {
     logger(LOG, "Method", "GET", fd);
     return Method::GET;
   }
   else if (strncmp(buffer, POST.c_str(), POST.size()) == 0 ||
            strncmp(buffer, post.c_str(), post.size()) == 0) {
     logger(LOG, "Method", "POST", fd);
     return Method::POST;
   }
   logger(FORBIDDEN, "Only simple GET operation supported", buffer, fd);
   exit(4);
   return Method::GET;
}

void parse_headers(const char* buffer, std::map<const char*, std::string>&
header) {
   int pos = 0;
   const char* request = buffer;
   while(request[0] != '\0' && pos < BUFSIZE) {
     const char* lf = strchr(request, '\n');
     if (lf == nullptr) {
       break;
     }
     pos = lf - request;
     const char* colon = strchr(request, ':');
     if (colon == nullptr) {
       break;
     }
     if ((lf - colon) > 0) {
       auto size = static_cast<std::string::size_type>(colon - request);
       std::string name{request, size};
       colon++;
       while (isspace(colon[0])) {
         colon++;
       }
       size = static_cast<std::string::size_type>(lf - colon);
       std::string value{colon, size};
       while (isspace(value.back())) {
         value.pop_back();
       }
       logger(LOG, "header", name + "=" + value, 0);
       header[name.c_str()] = value;
     }
     pos += (lf - request);
     request = lf + 1;
   }
}

void handle_get(char* buffer, std::map<const char*, std::string>& header,
                 int fd, int hit) {
   std::string::size_type len, buflen;
   int i;
   for (i = 4;i < BUFSIZE; i++) { /* null terminate after the second space */
     if (buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
       buffer[i] = 0;
       break;
     }
   }
   for (int j = 0; j < i-1; j++) { /* check for illegal parent directory use .. */
     if (buffer[j] == '.' && buffer[j+1] == '.') {
       logger(FORBIDDEN, "Parent directory (..) path names not supported",
              buffer, fd);
     }
   }
   if (!strncmp(&buffer[0], "GET /\0", 6) ||
       !strncmp(&buffer[0], "get /\0", 6) ) { /* convert to index file */
     strcpy(buffer, "GET /index.html");
   }
   /* work out the file type and check we support it */
   buflen = strlen(buffer);
   std::string fext;
   for (auto& extension : extensions) {
     len = strlen(extension.ext);
     if (!strncmp(&buffer[buflen-len], extension.ext, len)) {
       fext = extension.filetype;
       break;
     }
   }
   logger(LOG, "Extension", fext, fd);
   if (fext.empty()) {
     std::string path{&buffer[4], strlen(&buffer[4])};
     const auto& td = rest_data.find(path);
     if (td == rest_data.end()) {
       logger(NOTFOUND, "path not found", &buffer[5], fd);
     }
     else {
       std::ifstream file;
       file.open(rest_data[path]);
       if (!file) {  /* open the file for reading */
         logger(NOTFOUND, "failed to open file", &buffer[5], fd);
       }
       file.seekg(0, std::ios::end);
       std::streampos fsize = file.tellg();
       file.seekg(0, std::ios::beg);
       logger(LOG, "Send", &buffer[5], 0);
       std::ostringstream log;
       log << "HTTP/1.1 200 OK\nServer: mock_rest_api/" << VERSION << ".0\n"
           << "Content-Length: " << fsize << "\n"
           << "Connection: close\nContent-Type: " << fext << "\n\n";
       logger(HEADER, "Resoponse Header", log.str(), hit);
       write(fd, log.str().c_str(), log.str().size());

       /* send file in 8KB block - last block may be smaller */
       while (!file.eof()) {
         int ret = file.readsome(buffer, BUFSIZE);
         write(fd, buffer, ret);
       }
       sleep(1); /* allow socket to drain before closing */
       close(fd);
     }
   }
   else {
     std::ifstream file;
     file.open(&buffer[5]);
     if (!file) {  /* open the file for reading */
       logger(NOTFOUND, "failed to open file", &buffer[5], fd);
     }
     file.seekg(0, std::ios::end);
     std::streampos fsize = file.tellg();
     file.seekg(0, std::ios::beg);
     logger(LOG, "Send", &buffer[5], 0);
     std::ostringstream log;
     log << "HTTP/1.1 200 OK\nServer: mock_rest_api/" << VERSION << ".0\n"
         << "Content-Length: " << fsize << "\n"
         << "Connection: close\nContent-Type: " << fext << "\n\n";
     logger(HEADER, "Resoponse Header", log.str(), hit);
     write(fd, log.str().c_str(), log.str().size());

     /* send file in 8KB block - last block may be smaller */
     while (!file.eof()) {
       int ret = file.readsome(buffer, BUFSIZE);
       write(fd, buffer, ret);
     }
     sleep(1); /* allow socket to drain before closing */
     close(fd);
   }
}

void handle_post(char* buffer, const std::map<const char*, std::string>& header,
                  int fd, int hit) {
   int len, buflen;
   int i;
   for (i = 4;i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
     if (buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
       buffer[i] = 0;
       break;
     }
   }
   for (int j = 0; j < i-1; j++) { /* check for illegal parent directory use .. */
     if (buffer[j] == '.' && buffer[j+1] == '.') {
       logger(FORBIDDEN, "Parent directory (..) path names not supported", buffer, fd);
     }
   }
   /* work out the file type and check we support it */
   buflen = strlen(buffer);
   std::string fstr;
   for (auto& extension : extensions) {
     len = strlen(extension.ext);
     if (!strncmp(&buffer[buflen-len], extension.ext, len)) {
       fstr = extension.filetype;
       break;
     }
   }
   if (fstr.empty()) {
     logger(FORBIDDEN, "file extension type not supported", buffer, fd);
   }
   sleep(1); /* allow socket to drain before signalling the socket is closed */
   close(fd);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit) {
   long ret;
   static char buffer[BUFSIZE+1]; /* static so zero filled */

   ret = read(fd, buffer, BUFSIZE); /* read Web request in one go */
   if (ret == 0 || ret == -1) { /* read failure stop now */
     logger(FORBIDDEN, "failed to read browser request", "", fd);
   }
   if (ret > 0 && ret < BUFSIZE) { /* return code is valid chars */
     buffer[ret] = 0; /* terminate the buffer */
   }
   else {
     buffer[0] = 0;
   }
   logger(HEADER, "Request + Header", buffer, hit);
   Method method = parse_method(buffer, fd);
   std::map<const char*, std::string> header;
   parse_headers(buffer, header);
   switch (method) {
   case Method::GET:
     handle_get(buffer, header, fd, hit);
     break;
   case Method::POST:
     handle_post(buffer, header, fd, hit);
     break;
   default:
     logger(LOG, "Method", buffer, fd);
   }
   exit(1);
}

void *ThreadMain(void *threadArgs) {
    static int hit;
    // Guarantees that thread resources are deallocated upon return
    pthread_detach(pthread_self());

    // Extract socket file descriptor from argument
    int fd = ((struct ThreadArgs *) threadArgs)->clntSock;
    free(threadArgs); // Deallocate memory for argument

    long ret;
    static char buffer[BUFSIZE+1]; /* static so zero filled */

    ret = read(fd, buffer, BUFSIZE); /* read Web request in one go */
    if (ret == 0 || ret == -1) { /* read failure stop now */
        logger(FORBIDDEN, "failed to read browser request", "", fd);
    }
    if (ret > 0 && ret < BUFSIZE) { /* return code is valid chars */
        buffer[ret] = 0; /* terminate the buffer */
    }
    else {
        buffer[0] = 0;
    }
    logger(HEADER, "Request + Header", buffer, hit++);
    Method method = parse_method(buffer, fd);
    std::map<const char*, std::string> header;
    parse_headers(buffer, header);
    switch (method) {
      case Method::GET:
        handle_get(buffer, header, fd, hit);
        break;
      case Method::POST:
        handle_post(buffer, header, fd, hit);
        break;
      default:
        logger(LOG, "Method", buffer, fd);
    }

    return (NULL);
}

void PrintSocketAddress(const struct sockaddr *address, FILE *stream) {
  // Test for address and stream
  if (address == NULL || stream == NULL)
    return;

  void *numericAddress; // Pointer to binary address
  // Buffer to contain result (IPv6 sufficient to hold IPv4)
  char addrBuffer[INET6_ADDRSTRLEN];
  in_port_t port; // Port to print
  // Set pointer to address based on address family
  switch (address->sa_family) {
  case AF_INET:
    numericAddress = &((struct sockaddr_in *) address)->sin_addr;
    port = ntohs(((struct sockaddr_in *) address)->sin_port);
    break;
  case AF_INET6:
    numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
    port = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
    break;
  default:
    fputs("[unknown type]", stream);    // Unhandled type
    return;
  }
  // Convert binary to printable address
  if (inet_ntop(address->sa_family, numericAddress, addrBuffer,
      sizeof(addrBuffer)) == NULL)
    fputs("[invalid address]", stream); // Unable to convert
  else {
    fprintf(stream, "%s", addrBuffer);
    if (port != 0)                // Zero not valid in any socket addr
      fprintf(stream, "-%u", port);
  }
}

int web(int servSock) {
    for (;;) { // Run forever
        struct sockaddr_storage clntAddr; // Client address
        // Set length of client address structure (in-out parameter)
        socklen_t clntAddrLen = sizeof(clntAddr);

        // Wait for a client to connect
        int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
        if (clntSock < 0) {
            logger(ERROR, "accept() failed", "unkown", 0);
            exit(-1);
        }

        // clntSock is connected to a client!

        fputs("Handling client ", stdout);
        PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
        fputc('\n', stdout);

        // Create separate memory for client argument
        struct ThreadArgs *threadArgs = (struct ThreadArgs *) malloc(
            sizeof(struct ThreadArgs));
        if (threadArgs == NULL) {
            logger(ERROR, "malloc() failed", "", servSock);
            exit(-1);
        }
        threadArgs->clntSock = clntSock;

        // Create client thread
        pthread_t threadID;
        int returnValue = pthread_create(&threadID, NULL, ThreadMain, threadArgs);
        if (returnValue != 0) {
            logger(ERROR, "pthread_create() failed", strerror(returnValue), servSock);
        }

        std::ostringstream threadIDStr;
        threadIDStr << threadID;
        logger(LOG, "Handle Request with thread", threadIDStr.str(), 0);
    }
    // NOT REACHED
}

bool init_rest_data(const std::string& json_file) {
   boost::property_tree::ptree ptree;
   std::ifstream ifs(json_file);
   if (ifs) {
     try {
       boost::property_tree::read_json(ifs, ptree);
     }
     catch (std::exception ex) {
       std::cerr << ex.what() << std::endl;
       exit(-1);
     }
   }
   else {
     std::cerr << json_file << " not found." << std::endl;
     exit(-1);
   }
   const auto& requests = ptree.get_child("requests");
   for (auto& field: requests) {
     const auto& path = field.second.get_optional<std::string>("path");
     const auto& response = field.second.get_optional<std::string>("response");
     if (path && response) {
       std::cout << "path=" << *path << " | response=" << *response << std::endl;
       if (rest_data.find(*response) == rest_data.end()) {
         rest_data[*path] = *response;
       }
       else {
         std::cerr << "Duplicate path '" << *path << "' found in "
                   << json_file << std::endl;
         exit(-3);
       }
     }
   }
   ifs.close();
   return true;
}
