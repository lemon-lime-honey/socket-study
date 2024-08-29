# 시스템 콜 또는 버스트

- 유닉스 박스의 네트워크 기능이나 해당 기능을 지원하는 소켓 API를 지원하는 박스의 네트워크 기능에 접근하게 해주는 시스템 콜(그리고 다른 라이브러리 콜)을 다룬다.
- 이 함수 중 하나를 호출하면 커널이 자동으로 일을 한다.
- 이 부분에서 대부분의 사람들이 막히는 지점은 이것들을 어떤 순서로 호출하는가이다.

(아래의 많은 코드 조각은 필요한 오류 체크를 하지 않았다. 그리고 보통 `getaddrinfo()`의 호출이 성공해 연결 리스트의 유효한 시작점을 반환한다고 가정한다. 이런 상황들은 모두 stand-alone 프로그램에서 기술되었기 때문에 그런 프로그램들을 모델로 사용하라.)

## `getaddrinfo()` - 동작 준비

- DNS와 서비스명 참조, 필요한 구조체 채우기를 `getaddrinfo()`가 수행한다.

```c
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int getaddrinfo(const char *node,     // e.g. "www.example.com" or IP
                const char *service,  // e.g. "http" or port number
                const struct addrinfo *hints,
                struct addrinfo **res);
```

- 입력 인자가 세 개 필요하고, 결과로 연결 리스트 `res`의 포인터를 반환한다.
- `node` 파라미터는 연결할 호스트의 이름 또는 IP 주소이다.
- `service` 파라미터는 "80"처럼 포트 번호가 될 수도 있고, "http", "ftp", "telnet" 또는 "smtp" 등과 같은 특정 서비스의 이름일 수도 있다.
  - [IANA 포트 목록](https://www.iana.org/assignments/port-numbers) 또는 유닉스 기기의 `etc/services`에서 찾을 수 있다.
- `hints` 파라미터는 연관된 정보를 이미 채운 `struct addrinfo`를 가리킨다.

- 다음은 호스트의 IP 주소의 3490번 포트를 listen하려는 서버의 샘플 호출이다.
- 이것이 정말로 listen을 하거나 네트워크 설정을 하는 건 아니다. 이후에 사용할 구조를 설정하는 것이다.

```c
int status;
struct addrinfo hints;
struct addrinfo *servinfo; // will point to the results

memset(&hints, 0, sizeof hints); // make sure the struct is empty
hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

if ((status = getaddrinfo(NULL, "3490", &hints, &servinfo)) != 0) {
  fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
  exit(1);
}

// servinfo now points to a linked list of 1 or more struct addrinfos

// ... do everything until you don't need servinfo anymore ....

freeaddrinfo(servinfo); // free the linked-list
```

- `ai_family`를 `AF_UNSPEC`으로 설정해 IPv4나 IPv6 중 어느 것을 사용하도 무관함을 표현했다. 둘 중 하나를 명시하고 싶다면 `AF_INET` 또는 `AF_INET6`으로 설정한다.
- `AI_PASSIVE` 플래그: `getaddrinfo()`에게 소켓 구조에 로컬 호스트의 주소를 할당하라고 하는 것
  - 하드코딩하지 않아도 되어 좋다.
  - 또는 `getaddrinfo()`의 첫 번째 인자에 구체적인 주소를 넣을 수도 있다.
- 그 다음 호출을 한다.
  - 오류가 발생한다면(`getaddrinfo()`가 0이 아닌 수를 반환한다.) `gai_strerror()` 함수를 사용해 출력한다.
  - 정상적으로 동작한다면 `servinfo`가 각각 `struct sockaddr`를 포함하는 `struct addrinfo`로 이루어진 연결 리스트를 가리키게 된다.
- 마지막으로, `getaddrinfo()`가 할당한 연결 리스트 사용이 끝났다면 `freeaddrinfo()`를 호출해 메모리를 해제한다.

- 다음은 구체적인 서버, 예를 들어 "www.example.net"의 3490번 포트에 연결하려는 클라이언트 입장의 예시이다.
- 실제로 연결을 하지는 않지만 이후에 사용한 구조를 설정하는 것이다.

```c
int status;
struct addrinfo hints;
struct addrinfo *servinfo;  // will point to the results

memset(&hints, 0, sizeof hints); // make sure the struct is empty
hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

// get ready to connect
status = getaddrinfo("www.example.net", "3490", &hints, &servinfo);

// servinfo now points to a linked list of 1 or more struct addrinfos

// etc.
```

- `servinfo`는 모든 종류의 주소 정보를 가지는 연결 리스트이다.
- 이 정보를 보여주는 빠른 데모 프로그램을 작성해보자.
- 이 짧은 프로그램은 명령줄에서 명시한 호스트의 IP 주소를 출력한다.

```c
/*
** showip.c -- show IP addresses for a host given on the command line
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char* argv[]) {
  struct addrinfo hints, *res, *p;
  int status;
  char ipstr[INET6_ADDRSTRLEN];

  if (argc != 2) {
    fprintf(stderr, "usage: showip hostname\n");
    return 1;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
  hints.ai_socktype = SOCK_STREAM;

  if ((status == getaddrinfo(argv[1], NULL, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 2;
  }

  printf("IP addresses for %s:\n\n:", argv[1]);

  for (p = res; p != NULL; p = p->ai_next) {
    void* addr;
    char* ipver;

      // get the pointer to the address itself,
      // different fields in IPv4 and IPv6:
      if (p->ai_family == AF_INET){ // IPv4
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
        addr = &(ipv4->sin_addr);
        ipver = "IPv4";
      } else { // IPv6
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
        addr = &(ipv6->sin6_addr);
        ipver = "IPv6";
      }

      // convert the IP to a string and print it:
      inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
      printf(" %s: %s\n", ipver, ipstr);
  }

  freeaddrinfo(res); // free the linked list

  return 0;
}
```

- 명령줄에 무언가 전달하면 `res`가 가리킨 연결 리스트를 채우는 `getaddrinfo()`를 호출한다. 그러면 리스트를 순회하며 내용을 출력하거나 다른 동작을 할 수 있다.

- 예시 동작 결과

```bash
$ showip www.example.net
IP address for www.example.net:
  IPv4: 192.0.2.88

$ showip ipv6.example.com
IP addresses for ipv6.example.com

  IPv4: 192.0.2.101
  IPv6: 2001:db8:8c00:22::171
```

- `getaddrinfo()`에서 얻은 결과를 다른 소켓 함수에 전달해 네트워크 연결을 완성할 것이다.

## `socket()` - 파일 기술자 얻기

```c
#include <sys/types.h>
#include <sys/socket.h>

int socket(int domain, int type, int protocol);
```

- `domain`: `PF_INET` 또는 `PF_INET6`
- `type`: `SOCK_STREAM` 또는 `SOCK_DGRAM`
- `protocol`
  - `0`이면 주어진 `type`을 위한 적절한 프로토콜 선택
  - `getprotobyname()`을 호출해 원하는 프로토콜을 참조할 수도 있다.
- `PF_INET`은 `AF_INET`과 가까운 관계이고, `struct sockaddr_in`의 `sin_family` 필드를 초기화할 때 사용할 수 있다.
  - 사실 이 둘은 너무 가까워서 정확히는 같은 값을 가지며, 많은 프로그래머들은 `socket()`을 호출하고 `PF_INET` 대신 `AF_INET`을 첫 번째 인자로 넘긴다.
  - `struct sockaddr_in`에 `AF_INET`을, `socket()`을 호출할 때 `PF_INET`을 사용하는 것이 정확하다.

- 다음은 `getaddrinfo()`의 호출 결과값을 사용해 `socket()`에 사용하는 예제이다.

```c
int s;
struct addrinfo hints, *res;

// do the lookup
// [pretend we already filled out the "hints" struct]
getaddrinfo("www.example.com", "http", &hints, &res);

// again, you should do error-checking on getaddrinfo(), and walk
// the "res" linked list looking for valid entries instead of just
// assuming the first one is good (like many of these examples do).
// See the section on client/server for real examples.

s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
```

- `socket()`은 이후의 시스템 콜에서 사용할 수 있는 소켓 기술자 또는 오류가 발생한 경우 `-1`을 반환한다.
- 전역변수 `errno`가 오류 값으로 설정된다.

## `bind()` - 내가 지금 어느 포트에 있더라

- 일단 소켓을 가지게 되면 로컬 기기의 포트에 그 소켓을 연결하고 싶을 것이다.
  - 보통은 특정 포트에 들어오는 연결에 `listen()`을 하게 되면 된다.
- 포트 번호는 커널이 특정 프로세스의 소켓 기술자에 들어오는 패킷을 매칭할 때 사용된다.
- `connect()`만 할 것이라면 이 부분은 불필요할 것이다.

```c
#include <sys/types.h>
#include <sys/socket.h>

int bind(int sockfd, struct sockaddr* my_addr, int addrlen);
```

- `sockfd`는 `socket()`이 반환하는 소켓 파일 기술자이다.
- `my_addr`은 주소, 즉 포트와 IP 주소에 관한 정보를 포함하는 `struct sockaddr`의 포인터이다.
- `addrlen`은 그 주소의 길이를 바이트로 나타낸 것이다.

- 다음은 프로그램이 동작 중인 호스트를 3490번 포트에 바인딩하는 예제이다.

```c
struct addrinfo hints, *res;
int sockfd;

// first, load up address structs with getaddrinfo():

memset(&hints, 0, sizeof hints);
hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
hints.ai_socktype = SOCK_STREAM;
hints.ai_flags = AI_PASSIVE; // fill in my IP for me

getaddrinfo(NULL, "3490", &hints, &res);

// make a socket:

sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

// bind it to the port we passed in to getaddrinfo():

bind(sockfd, res->ai_addr, res->ai_addrlen);
```

- `AI_PASSIVE` 플래그를 통해 프로그램이 동작 중인 호스트의 IP에 바인딩하도록 한다.
  - 특정한 로컬 IP 주소에 바인딩을 하고 싶다면 `AI_PASSIVE`를 빼고 `getaddrinfo()`의 첫 번째 인자에 IP 주소를 넣는다.
- `bind()` 또한 오류가 발생하면 `-1`을 반환하고 `errno`를 오류 값으로 설정한다.

- `bind()`를 호출할 때 주의
  - 슈퍼유저가 아니라면, 1024 아래의 모든 포트는 예약되어 있다.
  - 다른 프로그램이 이미 사용하고 있는 것이 아니라면 1024부터 65535까지의 포트 번호를 사용하면 된다.

- 서버를 재실행하고 `bind()`가 실패했을 때 "주소가 이미 사용 중"이라는 알림이 나타날 수 있다.
  - 연결되었던 소켓이 커널 어딘가에서 여전히 매달려 포트를 흔들고 있다는 것이다.
  - 정리가 될 때까지 잠깐 기다리거나, 다음과 같은 코드를 사용해 프로그램이 포트를 재사용할 수 있도록 허용하게 할 수 있다.

    ```c
    int yes =- 1;

    // lose the pesky "Address already in use" error message
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
      perror("setsockopt");
      exit(1);
    }
    ```

- `bind()`가 관한 추가적인 내용
  - 호출할 필요가 전혀 없을 때도 있다.
  - 원격 기기에 `connect()`를 하는 중이고 로컬 포트가 몇 번인지 중요하지 않다면 간단히 `connect()`를 호출할 수 있다.
    - 그러면 소켓이 바인딩되어 있지 않은지 확인하고 필요하다면 사용되지 않은 로컬 포트에 `bind()`할 것이다.

## `connect()` - 거기 너!

1. telnet 애플리케이션에 사용자가 소켓 파일 기술자를 얻기 위해 명령어를 입력했다.
2. 애플리케이션이 `socket()`을 호출한다.
3. 사용자가 "`10.12.110.57`"의 "`23`"번 포트에 연결하라고 한다.
4. 그 다음은?: `connect()`

```c
#include <sys/types.h>
#include <sys/socket.h>

int connect(int sockfd, struct sockaddr* serv_addr, int addrlen);
```

- `sockfd`: `socket()`을 호출했을 때 반환되는 소켓 파일 기술자
- `serv_addr`: 목적지 포트와 IP 주소를 포함하는 `struct sockaddr`
- `addrlen`: 서버 주소 구조의 바이트 길이

위의 모든 정보는 `getaddrinfo()`의 호출 결과에서 얻을 수 있다.

- 다음은 `www.example.com`의 `3490`번 포트로 소켓 연결을 생성하는 예제이다.

```c
struct addrinfo hints, *res;
int sockfd;

// first, load up address structs with getaddrinfo():

memset(&hints, 0, sizeof hints);
hints.ai_family = AF_UNSPEC;
hints.ai_socktype = SOCK_STREAM;

getaddrinfo("www.example.com", "3490", &hints, &res);

// make a socket:

sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

// connect!

connect(sockfd, res->ai_addr, res->ai_addrlen);
```

- `connect()`의 반환값을 확인해야 한다. 오류가 발생하면 `errno` 변수를 설정하고 `-1`을 반환한다.
- `bind()`를 호출하지 않은 점에 주목
  - 보통은 로컬 포트 번호를 신경쓰지 않고, 목적지(원격 포트)에 관심을 가진다.
  - 로컬 포트는 커널이 선택할 것이고, 우리가 연결할 사이트는 자동으로 이 정보를 받게 된다.

## `listen()` - 누가 저를 좀 불러 주시겠어요?

- 원격 호스트에 연결하는게 아니라 들어오는 연결을 기다리고 그걸 다뤄야 한다면?
- `listen()` :arrow_right: `accept()`

```c
int listen(int sockfd, int backlog);
```

- `sockfd`: `socket()` 시스템 호출이 반환하는 일반적인 소켓 파일 기술자
- `backlog`: 입력 큐에 허용되는 연결의 숫자
  - 들어오는 연결은 `accept()`가 될 때까지 이 큐에서 대기한다.
  - 이 변수는 큐에 대기할 수 있는 양의 한계값을 나타낸다.
  - 대부분의 시스템은 20 정도로 제한한다.
- `listen()` 또한 오류가 발생하면 `errno`를 설정하고 `-1`을 반환한다.

- 서버가 특정한 포트에서 동작할 수 있도록 `listen()`을 호출하기 전에 `bind()`를 호출해야 한다.
- 그러므로 들어오는 연결을 `listen`하려면 다음과 같은 순서로 시스템 호출을 해야한다.

```c
getaddressinfo();
socket();
bind();
listen();
/* accept() goes here */
```

## `accept()` - 포트 3490을 호출해주셔서 감사합니다

- 아주 멀리 있는 누군가가 당신 기기의 `listen()` 중인 포트에 `connect()`를 시도한다.
- 연결은 `accept()`가 될 때까지 큐에서 대기할 것이다.
- 당신은 `accept()`를 호출하고 연결을 보류하라고 할 것이다.
- 이 하나의 연결에 사용하기 위한 *새로운 소켓 파일 기술자*가 반환된다!
- 한 개의 비용으로 *두 개의 소켓 파일 기술자*를 가지게 된다!
- 기존의 것은 여전히 새로운 연결을 위해 listen
- 새로운 것은 드디어 `send()`와 `recv()`를 할 준비가 되었다.

```c
#include <sys/types.h>
#include <sys/socket.h>

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
```

- `sockfd`: `listen()` 중인 소켓 기술자
- `addr`: 로컬 `struct sockaddr_storage`의 포인터
  - 들어오는 요청에 대한 정보의 목적지
  - 어느 호스트나 어느 포트에서 호출 중인지 알 수 있다.
- `addrlen`: `struct sockaddr_storage`가 `accept()`에 전달되기 전에 `sizeof(struct sockaddr_storage)`로 설정되어야 하는 지역 정수 변수
  - `accept()`는 `addr`에 이보다 더 많은 바이트를 넣지는 않을 것이다.
  - 더 적게 넣는다면 그를 반영하기 위해 `addrlen`의 값을 변경할 것이다.
- `accept()` 또한 오류가 발생하면 `errno`를 설정하고 `-1`을 반환한다.

-다음은 사용 예시이다.

```c
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MYPORT "3490" // the port users will be connecting to
#define BACKLOG 10    // how many pending connections queue will hold

int main(void) {
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints, *res;
  int sockfd, new_fd;

  // !! don't forget your error checking for these calls !!

  // first, load up address structs with getaddrinfo():

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me

  getaddrinfo(NULL, MYPORT, &hints, &res);

  // make a socket, bind it, and listen to it:

  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  bind(sockfd, res->ai_addr, res->ai_addrlen);
  listen(sockfd, BACKLOG);

  // now accept an incoming connection:

  addr_size = sizeof their_addr;
  new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &addr_size);

  // ready to communicate on socket descriptor new_fd!
  .
  .
  .
}
```

- 소켓 기술자 `new_fd`는 `send()`와 `recv()` 호출에 사용될 것이다.
- 오직 하나의 연결만 받게 된다면 같은 포트에 연결이 들어오는 것을 방지하기 위해 `sockfd` listen을 `close()`할 수도 있다.

## `send()`, `recv()` - 말을 좀 해봐!

- 스트림 소켓이나 연결된 데이터그램 소켓과 통신하기 위한 함수
- 일반적인 비연결 데이터그램 소켓을 사용하고 싶다면 `sendto()`나 `recvfrom()`을 확인한다.

### `send()`

```c
int send(int sockfd, const void* msg, int len, int flags);
```

- `sockfd`: 목적지의 소켓 기술자
  - `socket()`에서 반환된 것, `accept()`로 얻은 것 모두 사용 가능
- `msg`: 보내려는 데이터의 포인터
- `len`: 보내려는 데이터의 바이트 길이
- `flags`: `0`으로 설정한다.

- 예시

```c
char *msg = "Obi-Wan was here!";
int len, bytes_sent;
.
.
.
len = strlen(msg);
bytes_sent = send(sockfd, msg, len, 0);
.
.
.
```

- `send()`: 송신한 바이트수를 반환. *보내라고 한 것보다 적을 수 있다.*
  - `send()`의 반환값과 `len` 값이 일치하지 않을 때 문자열의 나머지를 보내는 건 사용자에게 달린 문제다.
  - 패킷이 작다면(대략 1K보다 작은 정도) 한 번에 전체를 보낼 수 있을지도 모른다.
- 오류가 발생하면 `errno`를 오류값으로 설정하고 `-1`을 반환한다.

### `recv()`

```c
int recv(int sockfd, void* buf, int len, int flags);
```

- `sockfd`: 읽어들일 소켓 기술자
- `buf`: 정보를 읽어들일 버퍼
- `len`: 버퍼의 최대 길이
- `flags`: `0`으로 설정한다.
- 오류가 발생하면 `errno`를 설정하고 `-1`을 반환한다.
- 원격 쪽에서 연결을 닫으면 `0`을 반환한다.

## `sendto()`, `recvfrom()` - DGRAM으로 말을 좀 해 봐

### `sendto()`

데이터그램 소켓은 원격 호스트에 연결되어 있지 않기 때문에 패킷을 보내기 전에 목적지 주소를 먼저 전달해야 한다.

```c
int sendto(int sockfd, const void* msg, int len, unsigned int flags,
           const struct sockaddr* to, socklen_t tolen);
```

- 기본적으로 두 개의 인자가 추가된 `send()`와 같다.
- `to`: 목적지 IP 주소와 포트를 포함하는 `struct sockaddr`의 포인터
  - `sockaddr_in`, `sockaddr_in6`, 또는 캐스팅된 `sockaddr_storage`가 될 수 있다.
- `tolen`: `sizeof* to` 또는 `sizeof(struct sockaddr_storage)`로 설정할 수 있는 `int`

목적지 주소 구조체를 얻으려면 `getaddrinfo()` 또는 `recvfrom()`으로부터 얻거나 직접 내용을 채우면 된다.

`send()`처럼, `sendto()` 또한 송신한 바이트 수를 반환(보내라고 한 것보다 적을 수 있다!)하고, 오류가 발생했을 때에는 `-1`를 반환한다.

### `recvfrom()`

```c
int recvfrom(int sockfd, void* buf, int len, unsigned int flags,
             struct sockaddr* from, int* fromlen);
```

- 두 개의 인자가 추가된 `recv()`와 같다.
- `from`: 송신 기기의 IP 주소와 포트로 채워질 로컬 `struct sockaddr_storage`의 포인터
- `fromlen`: `sizeof* from` 또는 `sizeof(struct sockaddr_storage)`로 초기화 되어야 하는 지역 `int` 변수의 포인터
  - 함수가 반환될 때, `fromlen`은 `from`에 저장된 실제 길이를 가지게 된다.

`recvfrom()`은 수신된 바이트 수를 반환하거나 오류가 발생했을 때에는 `errno`를 설정하고 `-1`을 반환한다.

### 왜 `struct sockaddr_storage`인가?

왜 소켓 타입으로 `struct sockaddr_in`이 아니라 `struct sockaddr_storage`을 사용할까?  
:arrow_right: IPv4 또는 IPv6으로 한정하지 않기 위해 제네릭한 `struct sockaddr_storage`를 사용한다.

데이터그램 소켓을 `connect()`할 때에는 모든 통신에 간단히 `send()`와 `recv()`를 사용하면 된다. 소켓 자체는 여전히 데이터그램 소켓이며, 패킷은 여전히 UDP를 사용하지만 소켓 인터페이스가 자동으로 수신지와 발신지 정보를 추가할 것이다.

## `close()`, `shutdown()` - 저리 가!

일반적인 유닉스 파일 기술자 `close()` 함수를 사용한다.

```c
close(sockfd);
```

이렇게 하면 소켓에서 더이상 읽기나 쓰기를 할 수 없게 된다. 원격 측에서 소켓에 읽기나 쓰기 작업을 시도하면 오류가 발생할 것이다.

소켓이 어떻게 닫히는가를 좀 더 제어하고 싶다면 `shutdown()` 함수를 사용한다. 이는 `close()`처럼 양방향으로 통신을 끊거나, 특정 방향의 통신을 끊을 수 있게 해준다.

```c
int shutdown(int sockfd, int how);
```

- `sockfd`: 닫으려는 소켓 파일 기술자
- `how`: 다음 중 하나
  | `how` | 효과 |
  | --- | --- |
  | `0` | 이후 수신 불가 |
  | `1` | 이후 송신 불가 |
  | `2` | 이후 송신과 수신 불가 |
- `shutdown`은 성공하면 `0`, 오류가 발생하면 `errno`를 설정하고 `-1`을 반환한다.

비연결 데이터그램 소켓에 `shutdown()`을 쓰면 단순히 소켓이 `send()`나 `recv()`를 호출하지 못하게 된다. (데이터그램 소켓에 `connect()`를 사용하면 이 함수들을 사용할 수 있다.)

`shutdown()`은 실제로 파일 기술자를 닫지 않는다. 그저 사용성을 변경할 뿐이다. 소켓 기술자를 해제하려면 `close()`를 사용해야 한다.

(Windows와 Winsock를 사용한다면 `close()` 대신 `closesocket()`을 사용한다.)

## `getpeername()` - 누구세요?

연결된 스트림 소켓의 반대편에 누가 있는지를 알려준다.

```c
#include <sys/socket.h>

int getpeername(int sockfd, struct sockaddr* addr, int* addrlen);
```

- `sockfd`: 연결된 스트림 소켓의 기술자
- `addr`: 연결 상대의 정보를 가지는 `struct sockaddr` 또는 `struct sockaddr_in`의 포인터
- `addrlen`: `sizeof* addr` 또는 `sizeof(struct sockaddr)`로 초기화되어야 하는 `int`의 포인터
- 오류가 발생하면 `errno`를 설정하고 `-1`을 반환한다.

일단 주소를 얻게 되면 정보를 출력하거나 더 많은 정보를 얻기 위해 `inet_ntop()`, `getnameinfo()` 또는 `gethostbyaddr()`를 사용할 수 있다.

## `gethostname()` - 내가 누구게?

프로그램이 실행 중인 컴퓨터의 이름을 반환한다. 그 이름은 로컬 기기의 IP 주소를 결정하기 위해 `getaddrinfo()`에서 사용할 수 있다.

```c
#include <unistd.h>

int gethostname(char* hostname, size_t size);
```

- `hostname`: 함수가 반환될 때 호스트 이름을 저장하게 될 문자 배열의 포인터
- `size`: `hostname` 배열의 바이트 길이
- 성공하면 `0`, 오류가 발생하면 `errno`를 설정하고 `-1`을 반환한다.
