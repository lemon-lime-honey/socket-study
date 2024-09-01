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

## 부분적인 `send()` 다루기

[이 부분](./04-call.md/#send-recv---말을-좀-해봐)에서 `send()`가 요청한 바이트를 전부 전송하지 않을 수도 있다고 언급했다. 그렇다면 나머지 바이트에는 무슨 일이 생기는가?

송신을 기다리는 버퍼에 여전히 남아있다. 제어할 수 있는 범위 밖의 상황 때문에, 커널은 모든 데이터를 한 묶음으로 보내지 않기로 결정했고, 남은 데이터를 가져오는 건 당신에게 달렸다/

이러한 함수를 작성할 수 있다:

```c
#include <sys/types.h>
#include <sys/socket.h>

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 
```

- `s`: 데이터의 목적지 소켓
- `buf`: 데이터를 가지고 있는 버퍼
- `len`: 버퍼에 있는 데이터의 바이트 크기를 포함하는 `int`의 포인터
- 이 함수는 오류가 발생했을 때 `errno`를 설정하고 `-1`을 반환한다.
- 실제로 송신된 바이트 수는 `len`에 담겨 반환된다.
  - 오류가 발생하지 않았다면 송신을 요청한 바이트 크기와 같다.

다음은 함수 호출 예시이다.

```c
char buf[10] = "Anakin!";
int len;

len = strlen(buf);
if (sendall(s, buf, &len) == -1) {
  perror("sendall");
  printf("We only sent %d bytes because of the error!\n", len);
}
```

- 패킷 일부가 도착했을 때 수신 측은?
- 패킷이 가변 길이를 가진다면 수신자는 하나의 패킷이 언제 끝나고 다른 패킷이 언제 시작하는지 어떻게 알 수 있는가?

:arrow_right: *캡슐화*를 해야 할 수도...

## 직렬화 - 데이터를 패킹하는 법

문자열이 아니라 `int` 또는 `float`같은 이진 데이터를 보내려면 다음과 같은 방법이 가능하다.

1. `sprintf()` 같은 함수를 이용해 숫자를 문자열로 변환한 후 그 문자열을 송신한다. 수신 측은 `strtol()`과 같은 함수를 사용해 문자열을 다시 숫자로 파싱할 것이다.
2. `send()`에 데이터를 가리키는 포인터를 전달해 그냥 데이터 자체를 보낸다.
3. 이식성이 있는 이진 형식으로 숫자를 인코딩한다. 수신 측에서 디코딩한다.

(사실 이걸 실행하게 해주는 라이브러리들이 있다.)

사실 위의 세 방법 모두 장단점이 있지만 이 문서의 원 저자는 보통 세 번째 방법을 선호한다.

1. 첫 번째 방법

    - 선을 통해 들어오는 데이터를 쉽게 출력해 읽을 수 있다는 장점이 있다.
    - 때로 인간이 읽을 수 있는 프로토콜은 IRC(Internet Relay Chat) 같은 non-bandwidth-intensive 상황에서 사용하기 좋다.
    - 그러나 변환에 시간이 걸린다는 것과, 결과가 거의 언제나 원래 값보다 더 많은 공간을 차지한다는 단점이 있다.

2. 두 번째 방법

    - 쉽다.
    - 그냥 보낼 데이터의 포인터를 보낸다.

        ```c
        double d = 3490.15926535;

        send(s, &d, sizeof d, 0); /* DANGER--non-portable! */
        ```

    - 수신 측은 이걸 다음과 같이 받는다.

        ```c
        double d;

        recv(s, &d, sizeof d, 0); /* DANGER--non-portable! */
        ```

    - 쉽고, 단순하다.
    - 모든 아키텍처가 `double`(또는 `int`)를 같은 비트 표현으로 나타내거나 같은 바이트 순서를 가지지는 않는다.
    - 이 코드는 확실히 이식성이 없다.

3. 세 번째 방법

    - 데이터를 알려진 형식으로 패킹한 다음 그 결과를 보낸다.
    - 다음은 `float`를 패킹하는 방법의 예시이다.

        ```c
        #include <stdint.h>

        uint32_t htonf(float f)
        {
            uint32_t p;
            uint32_t sign;

            if (f < 0) { sign = 1; f = -f; }
            else { sign = 0; }
                
            p = ((((uint32_t)f)&0x7fff)<<16) | (sign<<31); // whole part and sign
            p |= (uint32_t)(((f - (int)f) * 65536.0f))&0xffff; // fraction

            return p;
        }

        float ntohf(uint32_t p)
        {
            float f = ((p>>16)&0x7fff); // whole part
            f += (p&0xffff) / 65536.0f; // fraction

            if (((p>>31)&0x1) == 0x1) { f = -f; } // sign bit set

            return f;
        }
        ```

    - 위의 코드는 `float`를 32비트 숫자에 저장하는 일종의 naive한 구현이다.
    - 최상위 비트(31)은 숫자의 부호("1"은 음수를 뜻함)를 저장하는데 사용되고, 그 다음 7개의 비트(30-16)은 `float`의 정수부를, 나머지 비트(15-0)은 소수부를 저장하는데 사용된다.

        ```c
        #include <stdio.h>

        int main(void)
        {
            float f = 3.1415926, f2;
            uint32_t netf;

            netf = htonf(f);  // convert to "network" form
            f2 = ntohf(netf); // convert back to test

            printf("Original: %f\n", f);        // 3.141593
            printf(" Network: 0x%08X\n", netf); // 0x0003243F
            printf("Unpacked: %f\n", f2);       // 3.141586

            return 0;
        }
        ```

    - 장점: 작고, 단순하고, 빠르다.
    - 단점: 공간을 효율적으로 활용하지 못하며, 범위가 심각하게 제한된다.
      - 위의 예시에서 마지막 두 자리는 정확하게 보존되지 않는다는 점을 알 수 있다.

- 다른 방법? 부동 소수점 숫자를 저장하는 표준은 `IEEE-754`로 알려져있다. 대부분은 부동 소수점 계산을 할 때 내부에서 이 형식을 사용하며, 그런 경우에, 엄밀히 말해서 변환을 할 필요는 없다.
- 그러나 소스코드가 이식성이 있기를 바란다면 그런 가정을 할 수 없다.
  - 한 편으로는, 속도가 중요하다면 변환이 필요없는 플랫폼에서는 이 과정을 제거하는 최적화를 해야한다. 그것이 바로 `htons()`와 유사한 함수들이 하는 일이다.

- 다음은 `IEEE-754` 형식으로 `float`와 `double`을 인코딩하는 예제이다. (대부분-NaN이나 Infinity는 인코딩하지 않지만, 그를 위해 변경될 수 있다.)

    ```c
    #define pack754_32(f) (pack754((f), 32, 8))
    #define pack754_64(f) (pack754((f), 64, 11))
    #define unpack754_32(i) (unpack754((i), 32, 8))
    #define unpack754_64(i) (unpack754((i), 64, 11))

    uint64_t pack754(long double f, unsigned bits, unsigned expbits)
    {
        long double fnorm;
        int shift;
        long long sign, exp, significand;
        unsigned significandbits = bits - expbits - 1; // -1 for sign bit

        if (f == 0.0) return 0; // get this special case out of the way

        // check sign and begin normalization
        if (f < 0) { sign = 1; fnorm = -f; }
        else { sign = 0; fnorm = f; }

        // get the normalized form of f and track the exponent
        shift = 0;
        while(fnorm >= 2.0) { fnorm /= 2.0; shift++; }
        while(fnorm < 1.0) { fnorm *= 2.0; shift--; }
        fnorm = fnorm - 1.0;

        // calculate the binary form (non-float) of the significand data
        significand = fnorm * ((1LL<<significandbits) + 0.5f);

        // get the biased exponent
        exp = shift + ((1<<(expbits-1)) - 1); // shift + bias

        // return the final answer
        return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
    }

    long double unpack754(uint64_t i, unsigned bits, unsigned expbits)
    {
        long double result;
        long long shift;
        unsigned bias;
        unsigned significandbits = bits - expbits - 1; // -1 for sign bit

        if (i == 0) return 0.0;

        // pull the significand
        result = (i&((1LL<<significandbits)-1)); // mask
        result /= (1LL<<significandbits); // convert back to float
        result += 1.0f; // add the one back on

        // deal with the exponent
        bias = (1<<(expbits-1)) - 1;
        shift = ((i>>significandbits)&((1LL<<expbits)-1)) - bias;
        while(shift > 0) { result *= 2.0; shift--; }
        while(shift < 0) { result /= 2.0; shift++; }

        // sign it
        result *= (i>>(bits-1))&1? -1.0: 1.0;

        return result;
    }
    ```

상단에 32비트(아마도 `float`)와 64비트(아마도 `double`) 숫자를 패킹하고 언패킹하는 매크로를 추가했지만, `pack754()` 함수를 직접 불러 `bit` 크기의 데이터를 인코딩할 수도 있다.(`expbits`만큼의 지수부가 보존될 것이다.)

다음은 예제이다.

```c
#include <stdio.h>
#include <stdint.h> // defines uintN_t types
#include <inttypes.h> // defines PRIx macros

int main(void)
{
    float f = 3.1415926, f2;
    double d = 3.14159265358979323, d2;
    uint32_t fi;
    uint64_t di;

    fi = pack754_32(f);
    f2 = unpack754_32(fi);

    di = pack754_64(d);
    d2 = unpack754_64(di);

    printf("float before : %.7f\n", f);
    printf("float encoded: 0x%08" PRIx32 "\n", fi);
    printf("float after  : %.7f\n\n", f2);

    printf("double before : %.20lf\n", d);
    printf("double encoded: 0x%016" PRIx64 "\n", di);
    printf("double after  : %.20lf\n", d2);

    return 0;
}
```

위의 코드는 다음과 같은 결과를 산출한다.

```text
float before : 3.1415925
float encoded: 0x40490FDA
float after  : 3.1415925

double before : 3.14159265358979311600
double encoded: 0x400921FB54442D18
double after  : 3.14159265358979311600
```

- `struct`를 패킹할 수 있을까?
- 불행히도, 컴파일러는 `struct`의 모든 곳에 자유롭게 패딩을 추가할 수 있고, 그것은 구조체 전체를 한 덩이로 보낼 수 없다는 것을 의미한다.
- `struct`를 전송하는 가장 좋은 방법은 각각의 필드를 독립적으로 패킹한 다음 반대쪽에 도착했을 때 `struct` 안으로 언팩하는 것이다.

Kernighan과 Pike의 `The Practice of Programming`에서, 그들은 `pack()`와 `unpack()`이라 불리는 정확히 이런 동작을 하는 `printf()`와 유사한 함수를 구현한다.

다음은 이 문서의 저자가 K&P의 방법을 변경한 코드이다.

<details>
<summary>펼쳐서 확인하기</summary>


<details>
<summary>`pack754()` 함수</summary>

```c
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// macros for packing floats and doubles:
#define pack754_16(f) (pack754((f), 16, 5))
#define pack754_32(f) (pack754((f), 32, 8))
#define pack754_64(f) (pack754((f), 64, 11))
#define unpack754_16(i) (unpack754((i), 16, 5))
#define unpack754_32(i) (unpack754((i), 32, 8))
#define unpack754_64(i) (unpack754((i), 64, 11))

/*
** pack754() -- pack a floating point number into IEEE-754 format
*/
unsigned long long int pack754(long double f, unsigned bits, unsigned expbits) {
  long double fnorm;
  int shift;
  long long sign, exp, significand;
  unsigned significandbits = bits - expbits - 1;  // -1 for sign bit

  if (f == 0.0) return 0;  // get this special case out of the way

  // check sign and begin normalization
  if (f < 0) {
    sign = 1;
    fnorm = -f;
  } else {
    sign = 0;
    fnorm = f;
  }

  // get the normalized form of f and track the exponent
  shift = 0;
  while (fnorm >= 2.0) {
    fnorm /= 2.0;
    shift++;
  }
  while (fnorm < 1.0) {
    fnorm *= 2.0;
    shift--;
  }
  fnorm = fnorm - 1.0;

  // calculate the binary form (non-float) of the significand data
  significand = fnorm * ((1LL << significandbits) + 0.5f);

  // get the biased exponent
  exp = shift + ((1 << (expbits - 1)) - 1);  // shift + bias

  // return the final answer
  return (sign << (bits - 1)) | (exp << (bits - expbits - 1)) | significand;
}

/*
** unpack754() -- unpack a floating point number from IEEE-754 format
*/
long double unpack754(unsigned long long int i, unsigned bits,
                      unsigned expbits) {
  long double result;
  long long shift;
  unsigned bias;
  unsigned significandbits = bits - expbits - 1;  // -1 for sign bit

  if (i == 0) return 0.0;

  // pull the significand
  result = (i & ((1LL << significandbits) - 1));  // mask
  result /= (1LL << significandbits);             // convert back to float
  result += 1.0f;                                 // add the one back on

  // deal with the exponent
  bias = (1 << (expbits - 1)) - 1;
  shift = ((i >> significandbits) & ((1LL << expbits) - 1)) - bias;
  while (shift > 0) {
    result *= 2.0;
    shift--;
  }
  while (shift < 0) {
    result /= 2.0;
    shift++;
  }

  // sign it
  result *= (i >> (bits - 1)) & 1 ? -1.0 : 1.0;

  return result;
}

/*
** packi16() -- store a 16-bit int into a char buffer (like htons())
*/
void packi16(unsigned char *buf, unsigned int i) {
  *buf++ = i >> 8;
  *buf++ = i;
}

/*
** packi32() -- store a 32-bit int into a char buffer (like htonl())
*/
void packi32(unsigned char *buf, unsigned long int i) {
  *buf++ = i >> 24;
  *buf++ = i >> 16;
  *buf++ = i >> 8;
  *buf++ = i;
}

/*
** packi64() -- store a 64-bit int into a char buffer (like htonl())
*/
void packi64(unsigned char *buf, unsigned long long int i) {
  *buf++ = i >> 56;
  *buf++ = i >> 48;
  *buf++ = i >> 40;
  *buf++ = i >> 32;
  *buf++ = i >> 24;
  *buf++ = i >> 16;
  *buf++ = i >> 8;
  *buf++ = i;
}

/*
** unpacki16() -- unpack a 16-bit int from a char buffer (like ntohs())
*/
int unpacki16(unsigned char *buf) {
  unsigned int i2 = ((unsigned int)buf[0] << 8) | buf[1];
  int i;

  // change unsigned numbers to signed
  if (i2 <= 0x7fffu) {
    i = i2;
  } else {
    i = -1 - (unsigned int)(0xffffu - i2);
  }

  return i;
}

/*
** unpacku16() -- unpack a 16-bit unsigned from a char buffer (like ntohs())
*/
unsigned int unpacku16(unsigned char *buf) {
  return ((unsigned int)buf[0] << 8) | buf[1];
}

/*
** unpacki32() -- unpack a 32-bit int from a char buffer (like ntohl())
*/
long int unpacki32(unsigned char *buf) {
  unsigned long int i2 = ((unsigned long int)buf[0] << 24) |
                         ((unsigned long int)buf[1] << 16) |
                         ((unsigned long int)buf[2] << 8) | buf[3];
  long int i;

  // change unsigned numbers to signed
  if (i2 <= 0x7fffffffu) {
    i = i2;
  } else {
    i = -1 - (long int)(0xffffffffu - i2);
  }

  return i;
}

/*
** unpacku32() -- unpack a 32-bit unsigned from a char buffer (like ntohl())
*/
unsigned long int unpacku32(unsigned char *buf) {
  return ((unsigned long int)buf[0] << 24) | ((unsigned long int)buf[1] << 16) |
         ((unsigned long int)buf[2] << 8) | buf[3];
}

/*
** unpacki64() -- unpack a 64-bit int from a char buffer (like ntohl())
*/
long long int unpacki64(unsigned char *buf) {
  unsigned long long int i2 = ((unsigned long long int)buf[0] << 56) |
                              ((unsigned long long int)buf[1] << 48) |
                              ((unsigned long long int)buf[2] << 40) |
                              ((unsigned long long int)buf[3] << 32) |
                              ((unsigned long long int)buf[4] << 24) |
                              ((unsigned long long int)buf[5] << 16) |
                              ((unsigned long long int)buf[6] << 8) | buf[7];
  long long int i;

  // change unsigned numbers to signed
  if (i2 <= 0x7fffffffffffffffu) {
    i = i2;
  } else {
    i = -1 - (long long int)(0xffffffffffffffffu - i2);
  }

  return i;
}

/*
** unpacku64() -- unpack a 64-bit unsigned from a char buffer (like ntohl())
*/
unsigned long long int unpacku64(unsigned char *buf) {
  return ((unsigned long long int)buf[0] << 56) |
         ((unsigned long long int)buf[1] << 48) |
         ((unsigned long long int)buf[2] << 40) |
         ((unsigned long long int)buf[3] << 32) |
         ((unsigned long long int)buf[4] << 24) |
         ((unsigned long long int)buf[5] << 16) |
         ((unsigned long long int)buf[6] << 8) | buf[7];
}

/*
** pack() -- store data dictated by the format string in the buffer
**
**   bits |signed   unsigned   float   string
**   -----+----------------------------------
**      8 |   c        C
**     16 |   h        H         f
**     32 |   l        L         d
**     64 |   q        Q         g
**      - |                               s
**
**  (16-bit unsigned length is automatically prepended to strings)
*/

unsigned int pack(unsigned char *buf, char *format, ...) {
  va_list ap;

  signed char c;  // 8-bit
  unsigned char C;

  int h;  // 16-bit
  unsigned int H;

  long int l;  // 32-bit
  unsigned long int L;

  long long int q;  // 64-bit
  unsigned long long int Q;

  float f;  // floats
  double d;
  long double g;
  unsigned long long int fhold;

  char *s;  // strings
  unsigned int len;

  unsigned int size = 0;

  va_start(ap, format);

  for (; *format != '\0'; format++) {
    switch (*format) {
      case 'c':  // 8-bit
        size += 1;
        c = (signed char)va_arg(ap, int);  // promoted
        *buf++ = c;
        break;

      case 'C':  // 8-bit unsigned
        size += 1;
        C = (unsigned char)va_arg(ap, unsigned int);  // promoted
        *buf++ = C;
        break;

      case 'h':  // 16-bit
        size += 2;
        h = va_arg(ap, int);
        packi16(buf, h);
        buf += 2;
        break;

      case 'H':  // 16-bit unsigned
        size += 2;
        H = va_arg(ap, unsigned int);
        packi16(buf, H);
        buf += 2;
        break;

      case 'l':  // 32-bit
        size += 4;
        l = va_arg(ap, long int);
        packi32(buf, l);
        buf += 4;
        break;

      case 'L':  // 32-bit unsigned
        size += 4;
        L = va_arg(ap, unsigned long int);
        packi32(buf, L);
        buf += 4;
        break;

      case 'q':  // 64-bit
        size += 8;
        q = va_arg(ap, long long int);
        packi64(buf, q);
        buf += 8;
        break;

      case 'Q':  // 64-bit unsigned
        size += 8;
        Q = va_arg(ap, unsigned long long int);
        packi64(buf, Q);
        buf += 8;
        break;

      case 'f':  // float-16
        size += 2;
        f = (float)va_arg(ap, double);  // promoted
        fhold = pack754_16(f);          // convert to IEEE 754
        packi16(buf, fhold);
        buf += 2;
        break;

      case 'd':  // float-32
        size += 4;
        d = va_arg(ap, double);
        fhold = pack754_32(d);  // convert to IEEE 754
        packi32(buf, fhold);
        buf += 4;
        break;

      case 'g':  // float-64
        size += 8;
        g = va_arg(ap, long double);
        fhold = pack754_64(g);  // convert to IEEE 754
        packi64(buf, fhold);
        buf += 8;
        break;

      case 's':  // string
        s = va_arg(ap, char *);
        len = strlen(s);
        size += len + 2;
        packi16(buf, len);
        buf += 2;
        memcpy(buf, s, len);
        buf += len;
        break;
    }
  }

  va_end(ap);

  return size;
}

/*
** unpack() -- unpack data dictated by the format string into the buffer
**
**   bits |signed   unsigned   float   string
**   -----+----------------------------------
**      8 |   c        C
**     16 |   h        H         f
**     32 |   l        L         d
**     64 |   q        Q         g
**      - |                               s
**
**  (string is extracted based on its stored length, but 's' can be
**  prepended with a max length)
*/
void unpack(unsigned char *buf, char *format, ...) {
  va_list ap;

  signed char *c;  // 8-bit
  unsigned char *C;

  int *h;  // 16-bit
  unsigned int *H;

  long int *l;  // 32-bit
  unsigned long int *L;

  long long int *q;  // 64-bit
  unsigned long long int *Q;

  float *f;  // floats
  double *d;
  long double *g;
  unsigned long long int fhold;

  char *s;
  unsigned int len, maxstrlen = 0, count;

  va_start(ap, format);

  for (; *format != '\0'; format++) {
    switch (*format) {
      case 'c':  // 8-bit
        c = va_arg(ap, signed char *);
        if (*buf <= 0x7f) {
          *c = *buf;
        }  // re-sign
        else {
          *c = -1 - (unsigned char)(0xffu - *buf);
        }
        buf++;
        break;

      case 'C':  // 8-bit unsigned
        C = va_arg(ap, unsigned char *);
        *C = *buf++;
        break;

      case 'h':  // 16-bit
        h = va_arg(ap, int *);
        *h = unpacki16(buf);
        buf += 2;
        break;

      case 'H':  // 16-bit unsigned
        H = va_arg(ap, unsigned int *);
        *H = unpacku16(buf);
        buf += 2;
        break;

      case 'l':  // 32-bit
        l = va_arg(ap, long int *);
        *l = unpacki32(buf);
        buf += 4;
        break;

      case 'L':  // 32-bit unsigned
        L = va_arg(ap, unsigned long int *);
        *L = unpacku32(buf);
        buf += 4;
        break;

      case 'q':  // 64-bit
        q = va_arg(ap, long long int *);
        *q = unpacki64(buf);
        buf += 8;
        break;

      case 'Q':  // 64-bit unsigned
        Q = va_arg(ap, unsigned long long int *);
        *Q = unpacku64(buf);
        buf += 8;
        break;

      case 'f':  // float
        f = va_arg(ap, float *);
        fhold = unpacku16(buf);
        *f = unpack754_16(fhold);
        buf += 2;
        break;

      case 'd':  // float-32
        d = va_arg(ap, double *);
        fhold = unpacku32(buf);
        *d = unpack754_32(fhold);
        buf += 4;
        break;

      case 'g':  // float-64
        g = va_arg(ap, long double *);
        fhold = unpacku64(buf);
        *g = unpack754_64(fhold);
        buf += 8;
        break;

      case 's':  // string
        s = va_arg(ap, char *);
        len = unpacku16(buf);
        buf += 2;
        if (maxstrlen > 0 && len > maxstrlen)
          count = maxstrlen - 1;
        else
          count = len;
        memcpy(s, buf, count);
        s[count] = '\0';
        buf += len;
        break;

      default:
        if (isdigit(*format)) {  // track max str len
          maxstrlen = maxstrlen * 10 + (*format - '0');
        }
    }

    if (!isdigit(*format)) maxstrlen = 0;
  }

  va_end(ap);
}

// #define DEBUG
#ifdef DEBUG
#include <assert.h>
#include <float.h>
#include <limits.h>
#endif

int main(void) {
#ifndef DEBUG
  unsigned char buf[1024];
  unsigned char magic;
  int monkeycount;
  long altitude;
  double absurdityfactor;
  char *s = "Great unmitigated Zot!  You've found the Runestaff!";
  char s2[96];
  unsigned int packetsize, ps2;

  packetsize = pack(buf, "CHhlsd", 'B', 0, 37, -5, s, -3490.5);
  packi16(buf + 1, packetsize);  // store packet size in packet for kicks

  printf("packet is %u bytes\n", packetsize);

  unpack(buf, "CHhl96sd", &magic, &ps2, &monkeycount, &altitude, s2,
         &absurdityfactor);

  printf("'%c' %hhu %u %ld \"%s\" %f\n", magic, ps2, monkeycount, altitude, s2,
         absurdityfactor);

#else
  unsigned char buf[1024];

  int x;

  long long k, k2;
  long long test64[14] = {0,
                          -0,
                          1,
                          2,
                          -1,
                          -2,
                          0x7fffffffffffffffll >> 1,
                          0x7ffffffffffffffell,
                          0x7fffffffffffffffll,
                          -0x7fffffffffffffffll,
                          -0x8000000000000000ll,
                          9007199254740991ll,
                          9007199254740992ll,
                          9007199254740993ll};

  unsigned long long K, K2;
  unsigned long long testu64[14] = {0,
                                    0,
                                    1,
                                    2,
                                    0,
                                    0,
                                    0xffffffffffffffffll >> 1,
                                    0xfffffffffffffffell,
                                    0xffffffffffffffffll,
                                    0,
                                    0,
                                    9007199254740991ll,
                                    9007199254740992ll,
                                    9007199254740993ll};

  long i, i2;
  long test32[14] = {0,
                     -0,
                     1,
                     2,
                     -1,
                     -2,
                     0x7fffffffl >> 1,
                     0x7ffffffel,
                     0x7fffffffl,
                     -0x7fffffffl,
                     -0x80000000l,
                     0,
                     0,
                     0};

  unsigned long I, I2;
  unsigned long testu32[14] = {
      0,           0,           1, 2, 0, 0, 0xffffffffl >> 1,
      0xfffffffel, 0xffffffffl, 0, 0, 0, 0, 0};

  int j, j2;
  int test16[14] = {0,      -0,     1,       2,       -1, -2, 0x7fff >> 1,
                    0x7ffe, 0x7fff, -0x7fff, -0x8000, 0,  0,  0};

  printf("char bytes: %zu\n", sizeof(char));
  printf("int bytes: %zu\n", sizeof(int));
  printf("long bytes: %zu\n", sizeof(long));
  printf("long long bytes: %zu\n", sizeof(long long));
  printf("float bytes: %zu\n", sizeof(float));
  printf("double bytes: %zu\n", sizeof(double));
  printf("long double bytes: %zu\n", sizeof(long double));

  for (x = 0; x < 14; x++) {
    k = test64[x];
    pack(buf, "q", k);
    unpack(buf, "q", &k2);

    if (k2 != k) {
      printf("64: %lld != %lld\n", k, k2);
      printf("  before: %016llx\n", k);
      printf("  after:  %016llx\n", k2);
      printf(
          "  buffer: %02hhx %02hhx %02hhx %02hhx "
          " %02hhx %02hhx %02hhx %02hhx\n",
          buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    } else {
      // printf("64: OK: %lld == %lld\n", k, k2);
    }

    K = testu64[x];
    pack(buf, "Q", K);
    unpack(buf, "Q", &K2);

    if (K2 != K) {
      printf("64: %llu != %llu\n", K, K2);
    } else {
      // printf("64: OK: %llu == %llu\n", K, K2);
    }

    i = test32[x];
    pack(buf, "l", i);
    unpack(buf, "l", &i2);

    if (i2 != i) {
      printf("32(%d): %ld != %ld\n", x, i, i2);
      printf("  before: %08lx\n", i);
      printf("  after:  %08lx\n", i2);
      printf(
          "  buffer: %02hhx %02hhx %02hhx %02hhx "
          " %02hhx %02hhx %02hhx %02hhx\n",
          buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    } else {
      // printf("32: OK: %ld == %ld\n", i, i2);
    }

    I = testu32[x];
    pack(buf, "L", I);
    unpack(buf, "L", &I2);

    if (I2 != I) {
      printf("32(%d): %lu != %lu\n", x, I, I2);
    } else {
      // printf("32: OK: %lu == %lu\n", I, I2);
    }

    j = test16[x];
    pack(buf, "h", j);
    unpack(buf, "h", &j2);

    if (j2 != j) {
      printf("16: %d != %d\n", j, j2);
    } else {
      // printf("16: OK: %d == %d\n", j, j2);
    }
  }

  if (1) {
    long double testf64[8] = {-3490.6677,  0.0,         1.0,     -1.0,
                              DBL_MIN * 2, DBL_MAX / 2, DBL_MIN, DBL_MAX};
    long double f, f2;

    for (i = 0; i < 8; i++) {
      f = testf64[i];
      pack(buf, "g", f);
      unpack(buf, "g", &f2);

      if (f2 != f) {
        printf("f64: %Lf != %Lf\n", f, f2);
        printf("  before: %016llx\n", *((long long *)&f));
        printf("  after:  %016llx\n", *((long long *)&f2));
        printf(
            "  buffer: %02hhx %02hhx %02hhx %02hhx "
            " %02hhx %02hhx %02hhx %02hhx\n",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
      } else {
        // printf("f64: OK: %f == %f\n", f, f2);
      }
    }
  }
  if (1) {
    double testf32[7] = {0.0, 1.0, -1.0, 10, -3.6677, 3.1875, -3.1875};
    double f, f2;

    for (i = 0; i < 7; i++) {
      f = testf32[i];
      pack(buf, "d", f);
      unpack(buf, "d", &f2);

      if (f2 != f) {
        printf("f32: %.10f != %.10f\n", f, f2);
        printf("  before: %016llx\n", *((long long *)&f));
        printf("  after:  %016llx\n", *((long long *)&f2));
        printf(
            "  buffer: %02hhx %02hhx %02hhx %02hhx "
            " %02hhx %02hhx %02hhx %02hhx\n",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
      } else {
        // printf("f32: OK: %f == %f\n", f, f2);
      }
    }
  }
  if (1) {
    float testf16[7] = {0.0, 1.0, -1.0, 10, -10, 3.1875, -3.1875};
    float f, f2;

    for (i = 0; i < 7; i++) {
      f = testf16[i];
      pack(buf, "f", f);
      unpack(buf, "f", &f2);

      if (f2 != f) {
        printf("f16: %f != %f\n", f, f2);
        printf("  before: %08x\n", *((int *)&f));
        printf("  after:  %08x\n", *((int *)&f2));
        printf(
            "  buffer: %02hhx %02hhx %02hhx %02hhx "
            " %02hhx %02hhx %02hhx %02hhx\n",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
      } else {
        // printf("f16: OK: %f == %f\n", f, f2);
      }
    }
  }
#endif

  return 0;
}
```

</details>
<details>

<summary>위의 코드를 사용하는 예제</summary>

```c
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
** packi16() -- store a 16-bit int into a char buffer (like htons())
*/
void packi16(unsigned char *buf, unsigned int i) {
  *buf++ = i >> 8;
  *buf++ = i;
}

/*
** packi32() -- store a 32-bit int into a char buffer (like htonl())
*/
void packi32(unsigned char *buf, unsigned long int i) {
  *buf++ = i >> 24;
  *buf++ = i >> 16;
  *buf++ = i >> 8;
  *buf++ = i;
}

/*
** packi64() -- store a 64-bit int into a char buffer (like htonl())
*/
void packi64(unsigned char *buf, unsigned long long int i) {
  *buf++ = i >> 56;
  *buf++ = i >> 48;
  *buf++ = i >> 40;
  *buf++ = i >> 32;
  *buf++ = i >> 24;
  *buf++ = i >> 16;
  *buf++ = i >> 8;
  *buf++ = i;
}

/*
** unpacki16() -- unpack a 16-bit int from a char buffer (like ntohs())
*/
int unpacki16(unsigned char *buf) {
  unsigned int i2 = ((unsigned int)buf[0] << 8) | buf[1];
  int i;

  // change unsigned numbers to signed
  if (i2 <= 0x7fffu) {
    i = i2;
  } else {
    i = -1 - (unsigned int)(0xffffu - i2);
  }

  return i;
}

/*
** unpacku16() -- unpack a 16-bit unsigned from a char buffer (like ntohs())
*/
unsigned int unpacku16(unsigned char *buf) {
  return ((unsigned int)buf[0] << 8) | buf[1];
}

/*
** unpacki32() -- unpack a 32-bit int from a char buffer (like ntohl())
*/
long int unpacki32(unsigned char *buf) {
  unsigned long int i2 = ((unsigned long int)buf[0] << 24) |
                         ((unsigned long int)buf[1] << 16) |
                         ((unsigned long int)buf[2] << 8) | buf[3];
  long int i;

  // change unsigned numbers to signed
  if (i2 <= 0x7fffffffu) {
    i = i2;
  } else {
    i = -1 - (long int)(0xffffffffu - i2);
  }

  return i;
}

/*
** unpacku32() -- unpack a 32-bit unsigned from a char buffer (like ntohl())
*/
unsigned long int unpacku32(unsigned char *buf) {
  return ((unsigned long int)buf[0] << 24) | ((unsigned long int)buf[1] << 16) |
         ((unsigned long int)buf[2] << 8) | buf[3];
}

/*
** unpacki64() -- unpack a 64-bit int from a char buffer (like ntohl())
*/
long long int unpacki64(unsigned char *buf) {
  unsigned long long int i2 = ((unsigned long long int)buf[0] << 56) |
                              ((unsigned long long int)buf[1] << 48) |
                              ((unsigned long long int)buf[2] << 40) |
                              ((unsigned long long int)buf[3] << 32) |
                              ((unsigned long long int)buf[4] << 24) |
                              ((unsigned long long int)buf[5] << 16) |
                              ((unsigned long long int)buf[6] << 8) | buf[7];
  long long int i;

  // change unsigned numbers to signed
  if (i2 <= 0x7fffffffffffffffu) {
    i = i2;
  } else {
    i = -1 - (long long int)(0xffffffffffffffffu - i2);
  }

  return i;
}

/*
** unpacku64() -- unpack a 64-bit unsigned from a char buffer (like ntohl())
*/
unsigned long long int unpacku64(unsigned char *buf) {
  return ((unsigned long long int)buf[0] << 56) |
         ((unsigned long long int)buf[1] << 48) |
         ((unsigned long long int)buf[2] << 40) |
         ((unsigned long long int)buf[3] << 32) |
         ((unsigned long long int)buf[4] << 24) |
         ((unsigned long long int)buf[5] << 16) |
         ((unsigned long long int)buf[6] << 8) | buf[7];
}

/*
** pack() -- store data dictated by the format string in the buffer
**
**   bits |signed   unsigned   float   string
**   -----+----------------------------------
**      8 |   c        C
**     16 |   h        H         f
**     32 |   l        L         d
**     64 |   q        Q         g
**      - |                               s
**
**  (16-bit unsigned length is automatically prepended to strings)
*/

unsigned int pack(unsigned char *buf, char *format, ...) {
  va_list ap;

  signed char c;  // 8-bit
  unsigned char C;

  int h;  // 16-bit
  unsigned int H;

  long int l;  // 32-bit
  unsigned long int L;

  long long int q;  // 64-bit
  unsigned long long int Q;

  float f;  // floats
  double d;
  long double g;
  unsigned long long int fhold;

  char *s;  // strings
  unsigned int len;

  unsigned int size = 0;

  va_start(ap, format);

  for (; *format != '\0'; format++) {
    switch (*format) {
      case 'c':  // 8-bit
        size += 1;
        c = (signed char)va_arg(ap, int);  // promoted
        *buf++ = c;
        break;

      case 'C':  // 8-bit unsigned
        size += 1;
        C = (unsigned char)va_arg(ap, unsigned int);  // promoted
        *buf++ = C;
        break;

      case 'h':  // 16-bit
        size += 2;
        h = va_arg(ap, int);
        packi16(buf, h);
        buf += 2;
        break;

      case 'H':  // 16-bit unsigned
        size += 2;
        H = va_arg(ap, unsigned int);
        packi16(buf, H);
        buf += 2;
        break;

      case 'l':  // 32-bit
        size += 4;
        l = va_arg(ap, long int);
        packi32(buf, l);
        buf += 4;
        break;

      case 'L':  // 32-bit unsigned
        size += 4;
        L = va_arg(ap, unsigned long int);
        packi32(buf, L);
        buf += 4;
        break;

      case 'q':  // 64-bit
        size += 8;
        q = va_arg(ap, long long int);
        packi64(buf, q);
        buf += 8;
        break;

      case 'Q':  // 64-bit unsigned
        size += 8;
        Q = va_arg(ap, unsigned long long int);
        packi64(buf, Q);
        buf += 8;
        break;

      case 'f':  // float-16
        size += 2;
        f = (float)va_arg(ap, double);  // promoted
        fhold = pack754_16(f);          // convert to IEEE 754
        packi16(buf, fhold);
        buf += 2;
        break;

      case 'd':  // float-32
        size += 4;
        d = va_arg(ap, double);
        fhold = pack754_32(d);  // convert to IEEE 754
        packi32(buf, fhold);
        buf += 4;
        break;

      case 'g':  // float-64
        size += 8;
        g = va_arg(ap, long double);
        fhold = pack754_64(g);  // convert to IEEE 754
        packi64(buf, fhold);
        buf += 8;
        break;

      case 's':  // string
        s = va_arg(ap, char *);
        len = strlen(s);
        size += len + 2;
        packi16(buf, len);
        buf += 2;
        memcpy(buf, s, len);
        buf += len;
        break;
    }
  }

  va_end(ap);

  return size;
}

/*
** unpack() -- unpack data dictated by the format string into the buffer
**
**   bits |signed   unsigned   float   string
**   -----+----------------------------------
**      8 |   c        C
**     16 |   h        H         f
**     32 |   l        L         d
**     64 |   q        Q         g
**      - |                               s
**
**  (string is extracted based on its stored length, but 's' can be
**  prepended with a max length)
*/
void unpack(unsigned char *buf, char *format, ...) {
  va_list ap;

  signed char *c;  // 8-bit
  unsigned char *C;

  int *h;  // 16-bit
  unsigned int *H;

  long int *l;  // 32-bit
  unsigned long int *L;

  long long int *q;  // 64-bit
  unsigned long long int *Q;

  float *f;  // floats
  double *d;
  long double *g;
  unsigned long long int fhold;

  char *s;
  unsigned int len, maxstrlen = 0, count;

  va_start(ap, format);

  for (; *format != '\0'; format++) {
    switch (*format) {
      case 'c':  // 8-bit
        c = va_arg(ap, signed char *);
        if (*buf <= 0x7f) {
          *c = *buf;
        }  // re-sign
        else {
          *c = -1 - (unsigned char)(0xffu - *buf);
        }
        buf++;
        break;

      case 'C':  // 8-bit unsigned
        C = va_arg(ap, unsigned char *);
        *C = *buf++;
        break;

      case 'h':  // 16-bit
        h = va_arg(ap, int *);
        *h = unpacki16(buf);
        buf += 2;
        break;

      case 'H':  // 16-bit unsigned
        H = va_arg(ap, unsigned int *);
        *H = unpacku16(buf);
        buf += 2;
        break;

      case 'l':  // 32-bit
        l = va_arg(ap, long int *);
        *l = unpacki32(buf);
        buf += 4;
        break;

      case 'L':  // 32-bit unsigned
        L = va_arg(ap, unsigned long int *);
        *L = unpacku32(buf);
        buf += 4;
        break;

      case 'q':  // 64-bit
        q = va_arg(ap, long long int *);
        *q = unpacki64(buf);
        buf += 8;
        break;

      case 'Q':  // 64-bit unsigned
        Q = va_arg(ap, unsigned long long int *);
        *Q = unpacku64(buf);
        buf += 8;
        break;

      case 'f':  // float
        f = va_arg(ap, float *);
        fhold = unpacku16(buf);
        *f = unpack754_16(fhold);
        buf += 2;
        break;

      case 'd':  // float-32
        d = va_arg(ap, double *);
        fhold = unpacku32(buf);
        *d = unpack754_32(fhold);
        buf += 4;
        break;

      case 'g':  // float-64
        g = va_arg(ap, long double *);
        fhold = unpacku64(buf);
        *g = unpack754_64(fhold);
        buf += 8;
        break;

      case 's':  // string
        s = va_arg(ap, char *);
        len = unpacku16(buf);
        buf += 2;
        if (maxstrlen > 0 && len >= maxstrlen)
          count = maxstrlen - 1;
        else
          count = len;
        memcpy(s, buf, count);
        s[count] = '\0';
        buf += len;
        break;

      default:
        if (isdigit(*format)) {  // track max str len
          maxstrlen = maxstrlen * 10 + (*format - '0');
        }
    }

    if (!isdigit(*format)) maxstrlen = 0;
  }

  va_end(ap);
}
```

</details>
</details>

다음은 약간의 데이터를 `buf`에 넣어 패킹하고 변수에 언팩하는, 위의 코드를 시연하는 코드이다. 문자열 인수(형식 지정자 "s")와 함께 `unpack()`를 호출할 때, `96s`처럼 버퍼 오버런을 방지하기 위해 최대 길이를 앞에 붙이는 것이 좋다. 네트워크를 통해 받은 데이터를 언팩할 때에는 주의한다. 악의적인 사용자가 시스템을 공격하기 위해 나쁘게 구성된 패킷을 보낼 수 있다.

```c
#include <stdio.h>

// various bits for floating point types--
// varies for different architectures
typedef float float32_t;
typedef double float64_t;

int main(void)
{
    unsigned char buf[1024];
    int8_t magic;
    int16_t monkeycount;
    int32_t altitude;
    float32_t absurdityfactor;
    char *s = "Great unmitigated Zot! You've found the Runestaff!";
    char s2[96];
    int16_t packetsize, ps2;

    packetsize = pack(buf, "chhlsf", (int8_t)'B', (int16_t)0, (int16_t)37, 
            (int32_t)-5, s, (float32_t)-3490.6677);
    packi16(buf+1, packetsize); // store packet size in packet for kicks

    printf("packet is %" PRId32 " bytes\n", packetsize);

    unpack(buf, "chhl96sf", &magic, &ps2, &monkeycount, &altitude, s2,
        &absurdityfactor);

    printf("'%c' %" PRId32" %" PRId16 " %" PRId32
            " \"%s\" %f\n", magic, ps2, monkeycount,
            altitude, s2, absurdityfactor);

    return 0;
}
```

스스로 짠 코드를 쓰거나, 남이 쓴 코드를 쓰거나, 매번 각각의 비트를 패킹하는 것보다는 버그를 쉽게 찾아내기 위해 일반적인 데이터 패킹 루틴을 따르는 것이 좋다.

## 데이터 캡슐화

- 가장 간단한 경우, 데이터 캡슐화는 식별 정보나 패킷 길이, 혹은 둘 다를 포함하는 헤더를 붙이는 것이다.
- 헤더는 프로젝트를 완성하는데 필요한 것을 나타내는 이진 데이터이다.

`SOCK_STREAM`을 사용하는 다중 사용자 채팅 프로그램이 있다고 하자. 사용자가 무언가를 입력했을 때, 다음 두 종류의 정보가 서버에 전달되어야 한다: 내용과 발화자.

- 메시지가 가변 길이라는 점이 문제다. "tom"이라는 사람이 "Hi"라고 한다. "Benjamin"이라는 다른 사람이 이렇게 말한다. "Hey guys what is up?"
- 그래서 당신은 들어오는 대로 이 모든 것을 클라이언트에 `send()`한다. 나가는 데이터 스트림은 이렇게 생겼다:

    ```text
    t o m H i B e n j a m i n H e y g u y s w h a t i s u p ?
    ```

- 이러면 클라이언트가 어떻게 한 메시지가 시작하고 끝나는지 알 수 있을까?
- 모든 메시지의 길이를 같게 한 다음 [위](#부분적인-send-다루기)에서 구현한 `sendall()`을 호출해도 된다.
- 하지만 이렇게 하면 대역폭을 낭비하게 된다. "tom"이 "Hi"라고 말할 수 있게 하기 위해 1024 바이트를 `send()`하는 건 낭비다.
- 그래서 우리는 데이터를 작은 헤더와 패킷 구조에 *캡슐화*시킨다.
- 클라이언트와 서버 모두 이 데이터를 어떻게 패킹하고 언패킹하는지 안다.

### 클라이언트-서버 통신 프로토콜 정의하기

사용자 이름이 `'\0'`으로 남은 부분이 채워지는 8자의 고정 길이라고 가정하자. 그리고 데이터가 최대 128자의 가변 길이라고 가정하자. 이런 상황에서 사용할 수 있는 단순한 패킷 구조를 살펴본다.

1. `len`(1바이트, 부호 없음): 8바이트 사용자 이름과 메시지 데이터를 세는 패킷의 총 길이.
2. `name`(8바이트): 사용자의 이름. 필요한 경우 NUL로 빈 자리가 채워진다.
3. `chatdata`(n바이트): 최대 128바이트의 데이터. 패킷 길이는 이 데이터의 길이에 8을 더한 값으로 계산된다.

위의 패킷 정의를 사용하면, 첫 번째 패킷은 16진수와 ASCII를 사용해 다음 정보로 구성될 것이다.

```text
   0A     74 6F 6D 00 00 00 00 00   48 69
(length)  T  o  m     (padding)     H  i
```

두 번째 패킷도 비슷하다.

```text
   18     42 65 6E 6A 61 6D 69 6E   48 65 79 20 67 75 79 73 20 77 ...
(length)  B  e  n  j  a  m  i  n    H  e  y     g  u  y  s     w  ...
```

(길이는 당연히 네트워크 바이트 순서로 저장된다. 이 경우, 1바이트이므로 상관이 없지만, 보통은 패킷에 모든 이진 정수를 네트워크 바이트 순서로 저장한다.)

이 데이터를 송신할 때, 안전해야 하므로 위의 [`sendall()`](#부분적인-send-다루기)과 같은 명령을 사용해 모두 다 보낼 때 `send()`를 여러 번 호출하더라도 모든 데이터가 송신되었다는 걸 확실하게 해야 한다.

비슷하게, 이 데이터를 수신할 때 약간 더 일을 해야 한다. 안전하려면, 패킷 일부를 수신할 수 있다고 가정해야 한다. 패킷을 전부 다 받을 때까지 `recv()`를 계속해서 호출해야 한다.

패킷 가장 앞에 패킷이 완전히 다 도착할 때까지 받아야 할 총 바이트가 있다. 또한 최대 패킷 크기가 1+8+128, 즉 137바이트라는 것도 안다.

여기서 할 수 있는 것은 두 가지이다.

1. 첫 번째
    - 모든 패킷이 길이로 시작한다는 것을 알기 때문에, 패킷 길이를 얻기 위해 `recv()`를 호출할 수 있다.
    - 길이를 알게 된 후로는 패킷 전체를 받을 때까지 다시 호출을 반복해 패킷의 남은 길이를 정확히 구한다.
    - 장점: 한 개의 패킷을 다 담을 정도로 충분히 큰 버퍼를 하나 가지면 된다.
    - 단점: 모든 데이터를 받으려면 `recv()`를 최소 두 번 호출해야 한다.
2. 두 번째
    - `recv()`를 호출하고 패킷 내의 최대 바이트 수를 받기 원한다고 하면 된다.
    - 그 다음 무얼 받건 간에 버퍼 뒤에 붙이고, 패킷이 완전한지 확인한다.
    - 물론 다음 패킷의 일부까지 받을 수 있기 때문에 그를 위한 공간이 필요하다.
    - 두 개의 패킷을 담기에 충분히 큰 배열을 하나 선언하면 된다.
      - 이것이 패킷이 도착하는대로 재조립하는 작업을 진행할 배열이다.
    - 데이터를 `recv()`할 때마다 작업 버퍼에 추가하고, 패킷이 완성되었는지 확인한다.
      - 버퍼 내의 바이트 수는 헤더에서 명시된 길이보다 크거나 같다(+1. 헤더 내의 길이는 길이 자체의 바이트를 포함하지 않는다).
      - 버퍼 내의 바이트 후가 1보다 작다면, 명백히 패킷이 완성되지 않았다.
        - 이 경우는 첫 번째 바이트가 쓰레기값이기 때문에 정확한 패킷 길이로 판정할 수 없어 따로 다루어야 한다.
    - 패킷이 완성되면 그 다음 작업을 진행한다. 사용하고, 작업 버퍼에서 지운다.
    - 한 번의 `recv()` 호출로 한 개의 패킷을 넘는 분량을 읽을 수도 있다.
      - 즉, 작업 버퍼에 하나의 완전한 패킷과 다음 패킷의 미완성된 부분이 있다.
      - 그러나 이런 일이 발생하기 때문에 *두 개*의 패킷을 담기 충분한 크기의 작업 버퍼를 만들어야 한다.
    - 헤더를 통해 첫 번째 패킷의 길이를 알고 있었고, 작업 버퍼에서의 바이트 수를 추적 중이었기 때문에 두 번째(미완성) 패킷에 속하는 양을 계산할 수 있다.
    - 첫 번째 패킷을 다룬 후 작업 버퍼에서 그 부분을 비우고 두 번째 패킷의 조각을 버퍼의 앞으로 가져와 다음 `recv()` 호출을 준비한다.  
      (두 번째 패킷의 조각을 버퍼 앞 쪽으로 옮기는 것에 시간이 걸리지만 환형 버퍼를 사용하면 그 작업이 필요하지 않다.)

## 패킷 브로드캐스트 - Hello, World!

지금까지 이 문서에서는 데이터를 하나의 호스트에서 또 다른 하나의 호스트로 전송하는 것에 관해 논했다. 그러나 *동시에* 여러 개의 호스트로 데이터를 보낼 수 있다.

UDP(TCP 안 됨)와 표준 IPv4에서 *브로드캐스팅*라고 불리는 메커니즘을 통해 할 수 있다. IPv6에서는 브로드캐스팅이 지원되지 않고, 더 상위의 기술인 *멀티캐스팅*을 사용해야 한다.

네트워크로 브로드캐스트 패킷을 보내려면 `SO_BROADCAST`라는 소켓 옵션을 설정해야 한다.

브로드캐스트 패킷을 사용하는 데에는 위험성이 따른다. 브로드캐스트 패킷을 받는 모든 시스템은 데이터의 목적지 포트를 찾을 때까지 데이터 캡슐화의 레이어를 벗겨내야 한다. 그리고 데이터를 넘기거나 버린다. 두 경우 모두 브로드캐스트 패킷을 받는 기기에서는 큰 작업이고, 모두 로컬 네트워크 위에 있기 때문에 수 많은 기기가 불필요한 작업을 해야 한다.

브로드캐스트 패킷을 보내는 데에는 한 가지 이상의 방법이 있다.  
다음은 일반적인 두 가지 방법이다.

1. 첫 번째
    - 데이터를 특정한 서브넷의 브로드캐스트 주소로 보낸다.
      - 주소의 호스트 부분 비트가 1로 설정된 서브넷 네트워크 주소
    - 예를 들어 집에서의 네트워크가 `192.168.1.0`이라면 넷마스크는 `255.255.255.0`이므로 주소의 마지막 바이트가 호스트 번호이다. (넷마스크에 의하면 첫 번째 세 바이트는 네트워크 번호)
      - 그러므로 브로드캐스트 주소는 `192.168.1.255`이다.
    - 유닉스에서 `ifconfig` 명령어는 이 데이터를 제공한다.
    - 궁금하다면: 브로드캐스트 주소를 얻기 위한 비트 연산은 `network_number` OR (NOT `netmask`)
    - 이 타입의 브로드캐스트 패킷을 로컬 네트워크 뿐만이 아니라 원격 네트워크에 보낼 수도 있지만, 이 경우 목적지의 라우터가 패킷을 무시할 가능성이 있다. (이런 패킷을 무시하지 않으면 브로드캐스트 통신을 과다하게 발송해 공격할 수 있다.)
2. 두 번째
    - 데이터를 "전역" 브로드캐스트 주소로 보낸다.
      - `255.255.255.255`, 통칭 `INADDR_BROADCAST`
    - 많은 기기는 자동으로 네트워크 번호와 이것을 비트연산 AND를 실행하여 네트워크 브로드캐스트 주소로 변환할 것이며, 몇몇은 그러지 않을 것이다.
    - 아이러니하게도 라우터는 이런 종류의 브로드캐스트 패킷을 로컬 네트워크로 전송하지 않는다.

`SO_BROADCAST` 소켓 옵션을 설정하지 않고 데이터를 브로드캐스트 주소로 전송하면? :arrow_right: [talker와 listener](./05-background.md/#데이터그램-소켓) 예제로 살펴보았다.

```bash
$ talker 192.168.1.2 foo
sent 3 bytes to 192.168.1.2
$ talker 192.168.1.255 foo
sendto: Permission denied
$ talker 255.255.255.255 foo
sendto: Permission denied
```

`SO_BROADCAST`를 설정한 후에는 원하는 곳 어디에는 `sendto()`를 할 수 있다.

이것이 브로드캐스트를 할 수 있는 UDP 애플리케이션과 그렇지 않은 애플리케이션의 유일한 차이이다.

다음은 `talker` 애플리케이션에 `SO_BROADCAST` 소켓 옵션을 추가한 예제이다.

```c
/*
** broadcaster.c -- a datagram "client" like talker.c, except
**                  this one can broadcast
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVERPORT 4950 // the port users will be connecting to

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in their_addr; // connector's address information
    struct hostent *he;
    int numbytes;
    int broadcast = 1;
    //char broadcast = '1'; // if that doesn't work, try this

    if (argc != 3) {
        fprintf(stderr,"usage: broadcaster hostname message\n");
        exit(1);
    }

    if ((he=gethostbyname(argv[1])) == NULL) {  // get the host info
        perror("gethostbyname");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // this call is what allows broadcast packets to be sent:
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast,
        sizeof broadcast) == -1) {
        perror("setsockopt (SO_BROADCAST)");
        exit(1);
    }

    their_addr.sin_family = AF_INET;     // host byte order
    their_addr.sin_port = htons(SERVERPORT); // short, network byte order
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);

    if ((numbytes=sendto(sockfd, argv[2], strlen(argv[2]), 0,
             (struct sockaddr *)&their_addr, sizeof their_addr)) == -1) {
        perror("sendto");
        exit(1);
    }

    printf("sent %d bytes to %s\n", numbytes,
        inet_ntoa(their_addr.sin_addr));

    close(sockfd);

    return 0;
}
```

이 코드와 "일반적인" UDP 클라이언트/서버 상황의 차이는 없다. (이 경우 클라이언트가 브로드캐스트 패킷을 보낼 수 있다는 예외가 있긴 하다.) UDP `listener` 프로그램 원본을 하나의 창에서 실행하고, 다른 창에서 `broadcaster`를 실행한다. 위에서 실패했던 전송을 이번에는 성공하게 될 것이다.

```bash
$ broadcaster 192.168.1.2 foo
sent 3 bytes to 192.168.1.2
$ broadcaster 192.168.1.255 foo
sent 3 bytes to 192.168.1.255
$ broadcaster 255.255.255.255 foo
sent 3 bytes to 255.255.255.255
```

그리고 `listener`가 패킷을 받았을 때 응답하는 것을 확인할 수 있다. (`listener`가 응답하지 않았다면, IPv6 주소로 고정되었기 때문일 수 있다. `listener.c`의 `AF_INET6`을 `AF_INET`으로 변경해 IPv4로 강제하는 것을 시도한다.)

다른 장치 중 같은 네트워크에 있는 것에서 `listener`를 실행해 각 장치에서 하나씩 `listener`가 실행되게 한 후 `broadcaster`에 브로드캐스트 주소를 넣고 다시 실행해본다. `sendto()`를 한 번만 실행했음에도 두 개의 `listener` 모두가 패킷을 받는다.

`listener`가 실행중인 장치로 보내는 데이터는 받지만 브로드캐스트 주소로 보내는 데이터는 받지 않는다면, 로컬 기기의 방화벽이 패킷을 막기 때문일 수 있다.

다시 언급하지만 브로드캐스트 패킷을 다룰 때에는 주의해야 한다. LAN에 있는 모든 장치들이 `recvfrom()` 수행 여부와 관계없이 패킷을 처리해야 하므로 전체 컴퓨터 네트워크에 상당한 부하를 줄 수 있다.
