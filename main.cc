#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <iostream>

#include "mock_rest_api.h"

using namespace std;

int main(int argc, char **argv) {
   int i, port, pid, listenfd, socketfd, hit;
   socklen_t length;
   static struct sockaddr_in cli_addr; /* static = initialised to zeros */
   static struct sockaddr_in serv_addr; /* static = initialised to zeros */

   if ( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
     std::cout << "hint: mock_rest_api Port-Number Top-Directory\t\tversion"
               << VERSION << "\n\n"
               << "\tmock_rest_api is a small and very safe mini web server\n"
               << "\tmock_rest_api only servers out file/web pages with extensions named below\n"
               << "\t and only from the named directory or its sub-directories.\n"
               << "\tThere is no fancy features = safe and secure.\n\n"
               << "\tExample: mock_rest_api 8181 /home/mock_rest_api\n\n"
               << "\tOnly Supports:" << std::endl;
     for (auto& extension : extensions)
       std::cout << extension.ext << " ";

     std::cout << "\n\tNot Supported: URLs including \"..\", Java,Javascript, CGI\n"
               << "\tNot Supported: directories ";
     for (auto& bad_dir : BAD_DIRS)
       std::cout << bad_dir << " ";
     std::cout << std::endl;
     exit(0);
   }

   for (auto& bad_dir : BAD_DIRS) {
     if (bad_dir == argv[2]) {
       std::cout << "ERROR: Bad top directory " << argv[2]
                 << ", see mock_rest_api -?" << std::endl;
       exit(3);
     }
   }

   if (chdir(argv[2]) == -1){
     std::cout << "ERROR: Can't Change to directory " << argv[2] << std::endl;
     exit(4);
   }
   /* Become deamon + unstopable and no zombies children ( = no wait()) */
   if (fork() != 0)
     return 0; /* parent returns OK to shell */
   signal(SIGCLD, SIG_IGN); /* ignore child death */
   signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
   for (i = 0;i<32;i++)
     close(i); /* close open files */
   setpgrp(); /* break away from process group */
   logger(LOG, "mock_rest_api starting", argv[1], getpid());
   /* setup the network socket */
   if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) <0)
     logger(ERROR, "system call", "socket", 0);
   port = atoi(argv[1]);
   if (port < 0 || port >60000)
     logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(port);
   if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <0)
     logger(ERROR, "system call", "bind", 0);
   if ( listen(listenfd, 64) <0)
     logger(ERROR, "system call", "listen", 0);
   for (hit = 1; ;hit++) {
     length = sizeof(cli_addr);
     if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
       logger(ERROR, "system call", "accept", 0);
     if ((pid = fork()) < 0) {
       logger(ERROR, "system call", "fork", 0);
     }
     else {
       if (pid == 0) { /* child */
         close(listenfd);
         web(socketfd, hit); /* never returns */
       }
       else { /* parent */
         close(socketfd);
       }
     }
   }
}
