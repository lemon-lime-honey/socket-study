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

#define MYPORT "4950"
#define MAXBUFLEN 100

using namespace std;

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  struct sockaddr_storage their_addr;
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
    cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      cerr << "listener: socket" << endl;
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      cerr << "listener: bind";
      continue;
    }

    break;
  }

  if (p == NULL) {
    cerr << "listener: failed to bindsocket" << endl;
    return 2;
  }

  freeaddrinfo(servinfo);
  cout << "listener: waiting to recvfrom..." << endl;

  addr_len = sizeof their_addr;
  if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                           (struct sockaddr*)&their_addr, &addr_len)) == -1) {
    cerr << "recvfrom" << endl;
    exit(1);
  }

  cout << "listener: got packet from "
       << inet_ntop(their_addr.ss_family,
                    get_in_addr((struct sockaddr*)&their_addr), s, sizeof s)
       << endl;
  cout << "listener: packet is " << numbytes << " bytes long" << endl;
  buf[numbytes] = '\0';
  cout << "listener: packet contains \"" << buf << "\"" << endl;

  close(sockfd);

  return 0;
}