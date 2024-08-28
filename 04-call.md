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
