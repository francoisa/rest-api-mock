#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iterator>

#include "httpd.h"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 2) {                 // Test for correct number of arguments  
    cerr << "Usage: " << argv[0] << " <Server Port> " << endl;
    exit(1);
  }

  unsigned short echoServPort = atoi(argv[1]);    // First arg:  local port  

  try {
    TCPServerSocket servSock(echoServPort);   // Socket descriptor for server  
  
    for (;;) {      // Run forever  
      // Create separate memory for client argument  
      TCPSocket *clntSock = servSock.accept();
  
      // Create client thread  
      pthread_t threadID;              // Thread ID from pthread_create()  
      if (pthread_create(&threadID, NULL, ThreadMain, 
              (void *) clntSock) != 0) {
        cerr << "Unable to create thread" << endl;
        exit(1);
      }
    }
  } catch (SocketException &e) {
    cerr << e.what() << endl;
    exit(1);
  }
  return 0;
}

int _main(int argc, char* argv[]) {
  default_random_engine seed((random_device())());
  string numStr;
  while (true) { 
    vector<int> mult1;
    cout << "Enter two numbers or 'q' to exit: ";
    cin >> numStr;
    if (numStr == "q") {
      break;
    }
    else {
      mult1.clear();
      int num = 0;
      istringstream issNum(numStr);
      for (int i = 0; i < num; ++i) {
        const int jump = uniform_int_distribution<int>{0, num-1}(seed);
        mult1.push_back(jump);
      }
    }
  }
  return 0;
}
