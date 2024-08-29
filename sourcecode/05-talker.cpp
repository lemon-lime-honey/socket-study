#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>

#define SERVERPORT "4950"

using namespace std;

int main(int argc, char* argv[]) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;

  if (argc != 3) {
    cerr << "usage: talker hostname message" << endl;
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0) {
    cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      cerr << "talker: socket";
      continue;
    }

    break;
  }

  if (p == NULL) {
    cerr << "talker: failed to create socket" << endl;
    return 2;
  }

  if ((numbytes = sendto(sockfd, argv[2], strlen(argv[2]), 0, p->ai_addr,
                         p->ai_addrlen)) == -1) {
    cerr << "talker: sendto";
    exit(1);
  }

  freeaddrinfo(servinfo);

  cout << "talker: send " << numbytes << " bytes to " << argv[1] << endl;
  close(sockfd);

  return 0;
}