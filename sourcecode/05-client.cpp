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

#define PORT "3490"
#define MAXDATASIZE 100

using namespace std;

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[]) {
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  if (argc != 2) {
    cerr << "usage: client hostname" << endl;
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
    cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      cerr << "client: socket" << endl;
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      cerr << "client: connect" << endl;
      continue;
    }

    break;
  }

  if (p == NULL) {
    cerr << "client: failed to connect" << endl;
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s,
            sizeof s);
  cout << "client: connecting to " << s << endl;

  freeaddrinfo(servinfo);

  if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
    cerr << "recv" << endl;
    exit(1);
  }

  buf[numbytes] = '\0';
  cout << "client: received '" << buf << "'" << endl;
  close(sockfd);

  return 0;
}