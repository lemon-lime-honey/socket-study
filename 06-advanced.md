# 약간 고급 기술

## Blocking

- `listener`를 실행했을 때 그냥 패킷이 올 때까지 기다린다.
  - `recvfrom()`를 호출함
  - 데이터가 오지 않음
  - `recvfrom()`이 데이터가 올 때까지 "block" 명령
- `accept()`, `recv()` 등의 함수도 블로킹을 함
- `socket()`으로 소켓 기술자를 생성할 때, 커널이 이 소켓을 블로킹 소켓으로 설정한다.
- 소켓이 블로킹을 하지 않기를 바란다면 `fcntl()`을 호출한다.

```c
#include <unistd.h>
#include <fcntl.h>
.
.
.
sockfd = socket(PF_INET, SOCK_STREAM, 0);
fcntl(sockfd, F_SETFL, O_NONBLOCK);
.
.
.
```

- 소켓을 논블로킹으로 설정하면 효과적으로 소켓을 조사할 수 있다. 논블로킹 소켓을 읽는데 데이터가 없다면, 블로킹이 허용되지 않기 때문에 `-1`을 반환하고 `errno`가 `EAGAIN` 또는 `EWOULDBLOCK`으로 설정될 것이다.
- 사실 이런 류의 조사는 별로 좋은 생각이 아니다. 만약 프로그램이 소켓 안의 데이터를 기다리며 바쁜 상태가 된다면 CPU 시간을 지나치게 사용하게 될 것이다.
- 대기 중인 데이터가 있는지 확인하려면 다음 절의 `poll()`을 사용하자.

## `poll()` - 동기 I/O 멀티플렉싱

- 한 번에 많은 소켓을 확인하고 데이터를 가진 것들을 다루는 법
  - 어느 것이 읽을 준비가 되었는지 확인하기 위해 계속 조사를 할 필요가 없다.

```text
주의
poll()은 연결 숫자가 아주 커지면 끔찍할 정도로 느려진다.
이 경우 시스템에서 사용 가능한 가장 빠른 방법을 사용하는
libevent와 같은 이벤트 라이브러리를 사용하는 것이 성능상 더 낫다.
```

- `poll()` 시스템 호출을 사용한다.
  - 운영체제에게 모든 어려운 일을 다 하게 하고, 어느 소켓에 데이터가 읽힐 준비가 되었는지 알려달라고 하는 것
  - 그 동안 프로세스는 시스템 자원을 절약할 수 있다.

- 모니터링할 소켓 기술자, 이벤트에 관한 정보가 있는 `struct pollfd` 배열을 유지하는 것이 전체적인 계획이다.
- OS는 그러한 이벤트가 발생할 때까지, 혹은 사용자가 명시한 제한 시간이 지날 때까지 `poll()` 호출을 블로킹할 것이다.

유용하게도 `listen()` 중인 소켓은 새로 들어오는 연결이 `accept()`될 준비가 되었을 때 "읽을 준비가 됨"을 반환한다.

```c
#include <poll.h>

int poll(struct pollfd fds[], nfds_t nfds, int timeout);
```

- `fds`: 정보 배열(모니터링할 소켓들)
- `nfds`: 배열 안의 원소 개수
- `timeout`: 제한 시간(단위: ms)
- 이벤트가 발생한 원소의 개수를 반환한다.

```c
struct pollfd {
  int fd;        // the socket descriptor
  short events;  // bitmap of events we're interested in
  short revents; // when poll() returns, bitmap of events that occurred
};
```

이 구조체의 배열을 가지게 될 것이고, 모니터링할 소켓 기술자를 각 원소의 `fd` 필드에 설정할 것이다. 그 다음 관심이 있는 이벤트의 종류를 나타내기 위해 `events` 필드를 설정한다.

`events` 필드는 다음의 비트연산 OR 값이다.

| 매크로 | 설명 |
| --- | --- |
| `POLLIN` | 이 소켓이 데이터를 `recv()`할 준비가 되었을 때 알림 |
| `POLLOUT` | 이 소켓에 블로킹 없이 데이터를 `send()`할 수 있을 때 알림 |

`struct pollfd` 배열이 준비되었다면, 배열의 크기, ms단위의 제한 시간 값고 함께 `poll()`에 전달한다.(음수로 지정해 영원히 기다리게 할 수도 있다.)

`poll()`이 반환하면 이벤트가 발생했다는 것을 의미하는  `POLLIN`이나 `POLLOUT`이 설정되었는지 보기 위해 `revents` 필드를 확인할 수 있다.

(사실 `poll()` 호출로 더 많은 일을 할 수 있다.)

다음은 표준 입력에서 데이터를 읽어들일 수 있을 때까지 2.5초를 기다리는 예제이다.

```c
// poll.c

#include <stdio.h>
#include <poll.h>

int main(void)
{
    struct pollfd pfds[1]; // More if you want to monitor more

    pfds[0].fd = 0;          // Standard input
    pfds[0].events = POLLIN; // Tell me when ready to read

    // If you needed to monitor other things, as well:
    //pfds[1].fd = some_socket; // Some socket descriptor
    //pfds[1].events = POLLIN;  // Tell me when ready to read

    printf("Hit RETURN or wait 2.5 seconds for timeout\n");

    int num_events = poll(pfds, 1, 2500); // 2.5 second timeout

    if (num_events == 0) {
        printf("Poll timed out!\n");
    } else {
        int pollin_happened = pfds[0].revents & POLLIN;

        if (pollin_happened) {
            printf("File descriptor %d is ready to read\n", pfds[0].fd);
        } else {
            printf("Unexpected event occurred: %d\n", pfds[0].revents);
        }
    }

    return 0;
}
```

- `poll()`은 이벤트가 발생한 `pfds` 배열 안의 원소 개수를 반환하지만 배열의 *어느* 원소인지는 알려주지 않는다.
- 그러나 얼마나 많은 `revents` 필드의 값이 0이 아닌지는 알 수 있다.
- 따라서 준비된 소켓을 찾기 위해 배열을 훑어야 하지만 순회 도중 반환된 만큼의 소켓이 준비가 되었다면 확인을 멈춰도 된다.

- `poll()`에 전달한 집합에 새로운 파일 기술자를 추가하려면?
  - 배열에 충분한 공간을 마련하거나, 필요한 만큼 추가로 공간은 `realloc()`한다.
- 집합에서 원소를 제거하는 것은?
  1. 배열의 마지막 요소를 삭제할 요소에 덮어 씌우고 `poll()`의 `count`에 1을 뺀 값을 전달한다.
  2. `fd` 필드를 음수로 설정하면 `poll()`이 해당 요소를 무시한다.
- 이 모든 걸 `telnet`할 수 있는 하나의 채팅 서버에 합치려면?
  1. listener 소켓을 생성하고, `poll()`에 전달되는 파일 기술자 집합에 추가한다.
      - 그 파일 기술자는 들어오는 연결이 있을 때 읽을 준비가 된 상태가 될 것이다.
  2. `struct pollfd` 배열에 새로운 연결을 추가한다.
  3. 공간이 모자라면 동적으로 배열의 크기를 키운다.
  4. 연결이 닫히면 배열에서 제거한다.
  5. 연결이 읽을 준비가 되었다면 데이터를 읽고 그 테이터를 다른 연결에 송신해 다른 사용자가 입력한 것을 볼 수 있게 한다.

- 다음 `poll server` 예제를 실행해보자.
- 하나의 창에서 실행하고, 여러 개의 다른 터미널 창에서 `telnet localhost 9034`를 실행한다.
- 창에 입력된 것을 다른 창에서도 볼 수 있게 된다.
- `telnet`에서 나가기 위해 `CTRL-]`을 치고 `quit`를 입력하면 서버가 단절을 감지하고 파일 기술자 배열에서 제거한다.

```c
/*
** pollserver.c -- a cheezy multiperson chat server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define PORT "9034"   // Port we're listening on

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Return a listening socket
int get_listener_socket(void)
{
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "pollserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai); // All done with this

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

// Main
int main(void)
{
    int listener;     // Listening socket descriptor

    int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    char buf[256];    // Buffer for client data

    char remoteIP[INET6_ADDRSTRLEN];

    // Start off with room for 5 connections
    // (We'll realloc as necessary)
    int fd_count = 0;
    int fd_size = 5;
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

    // Set up and get a listening socket
    listener = get_listener_socket();

    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    // Add the listener to set
    pfds[0].fd = listener;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection

    fd_count = 1; // For the listener

    // Main loop
    for(;;) {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }

        // Run through the existing connections looking for data to read
        for(int i = 0; i < fd_count; i++) {

            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN) { // We got one!!

                if (pfds[i].fd == listener) {
                    // If listener is ready to read, handle new connection

                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

                        printf("pollserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    // If not the listener, we're just a regular client
                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);

                    int sender_fd = pfds[i].fd;

                    if (nbytes <= 0) {
                        // Got error or connection closed by client
                        if (nbytes == 0) {
                            // Connection closed
                            printf("pollserver: socket %d hung up\n", sender_fd);
                        } else {
                            perror("recv");
                        }

                        close(pfds[i].fd); // Bye!

                        del_from_pfds(pfds, i, &fd_count);

                    } else {
                        // We got some good data from a client

                        for(int j = 0; j < fd_count; j++) {
                            // Send to everyone!
                            int dest_fd = pfds[j].fd;

                            // Except the listener and ourselves
                            if (dest_fd != listener && dest_fd != sender_fd) {
                                if (send(dest_fd, buf, nbytes, 0) == -1) {
                                    perror("send");
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}
```

## `select()` - 동기 I/O 멀티플렉싱, 옛 방식

```c
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int select(int numfds, fd_set* readfds, fd_set* writefds,
           fd_set* expectfds, struct timeval* timeout);
```

- 이 함수는 파일 기술자의 "집합", 정확히는 `readfds`, `writefds`, `exceptfds`를 모니터링한다.
- 만약 표준 입력과 `sockfd`라는 소켓 기술자로부터 읽기를 할 수 있는지 확인하고 싶다면 파일 기술자 `0`과 `sockfd`를 집합 `readfds`에 추가한다.
- 인자 `numfds`는 가장 큰 파일 기술자 값에 1을 더한 값이어야 한다
- `select()`가 반환할 때, `readfds`는 읽을 준비가 된 선택된 파일 기술자를 반영하도록 수정된다. 매크로 `FD_ISSET()`으로 시험해볼 수 있다.

| 함수 | 설명 |
| --- | --- |
| `FD_SET(int fd, fd_set* set) | `set`에 `fd` 추가 |
| `FD_CLR(int fd, fd_set* set) | `set`에서 `fd` 제거 |
| `FD_ISSET(int fd, fd_set* set) | `fd`가 `set`에 있으면 참 반환 |
| `FD_ZERO(fd_set* set) | `set` 비우기 |

- `struct timeval* timeout`은 제한 시간을 나타낸다.

```c
struct timeval {
  int tv_sec;  // seconds
  int tv_usec; // microseconds
}
```

- 함수가 반환할 때 `timeout`은 남은 시간으로 갱신될 수도 있다. 운영체제에 따라 다르다.
- `0`으로 설정하면 `select()`는 즉시 집합에서 모든 파일 기술자를 조사한다.
- `NULL`로 설정하면 무기한으로 첫 번째 파일 기술자가 준비될 때까지 기다린다.

```c
/*
** select.c -- a select() demo
*/

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define STDIN 0  // file descriptor for standard input

int main(void)
{
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 2;
    tv.tv_usec = 500000;

    FD_ZERO(&readfds);
    FD_SET(STDIN, &readfds);

    // don't care about writefds and exceptfds:
    select(STDIN+1, &readfds, NULL, NULL, &tv);

    if (FD_ISSET(STDIN, &readfds))
        printf("A key was pressed!\n");
    else
        printf("Timed out.\n");

    return 0;
} 
```

- 소켓이 연결을 끊는다면 `select()`는 그 소켓을 읽기 준비로 설정하며 반환한다. 해당 소켓에 `recv()`를 사용한다면 `0`을 반환할 것이므로, 이를 통해 클라이언트가 연결을 끊었다는 사실을 알 수 있다.
- 소켓이 `listen()` 중이라면 `readfds` 집합에 그 소켓의 파일 기술자를 넣어 새로운 연결이 있는지 확인할 수 있다.

- 다음 프로그램은 단순한 다중 사용자 채팅 서버처럼 동작한다.
- 하나의 창에서 실행하고, 복수의 여러 다른 창에서 `telnet`한다.
- `telnet hostname 9034`
- 하나의 `telnet` 세션에서 무언가를 입력하면 다른 창에 나타날 것이다.

```c
/*
** selectserver.c -- a cheezy multiperson chat server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "9034"   // port we're listening on

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[256];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client
                        for(j = 0; j <= fdmax; j++) {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) {
                                // except the listener and ourselves
                                if (j != listener && j != i) {
                                    if (send(j, buf, nbytes, 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}
```

- `master`: 새로운 연결을 위해 listen 중인 소켓 기줄자 뿐만이 아니라 현재 연결된 모든 소켓 기술자를 포함한다.
  - `select()`는 읽을 준비가 된 소켓을 반영해 전달한 집합은 *변화*시키기 때문에 생성
  - `select()`를 호출할 때마다 연결을 추적해야 하기 때문에 원본을 어딘가에 저장해야 했다.
  - `master`를 `read_fds`로 복사하고 `select()`를 호출했다.
  - 새로운 연결이 생길 때마다 `master` 집합에 추가해야 하고, 연결이 닫힐 때마다 `master` 집합에서 제거해야 한다.
