#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>

#define PORT "3490"
#define BACKLOG 10

using namespace std;

void sigchld_handler(int s) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0);
  errno = saved_errno;
}

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family = AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void) {
  int sockfd, new_fd;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      cerr << "server: socket" << endl;
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      cerr << "setsockopt" << endl;
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      cerr << "server: bind" << endl;
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL) {
    cerr << "server: failed to bind" << endl;
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    cerr << "listen" << endl;
    exit(1);
  }

  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    cerr << "sigaction" << endl;
    exit(1);
  }

  cout << "server: waiting for connections..." << endl;

  while (true) {
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
    if (new_fd == -1) {
      cerr << "accept" << endl;
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr),
              s, sizeof s);
    cout << "server: got connection from " << s << endl;

    if (!fork()) {
      close(sockfd);
      if (send(new_fd, "Hello, world!", 13, 0) == -1) {
        cerr << "send" << endl;
      }
      close(new_fd);
      exit(0);
    }
    close(new_fd);
  }

  return 0;
}