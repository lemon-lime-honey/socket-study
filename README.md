[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)

<details>
<summary>환경에 따른 유의사항 - Windows</summary>

- 언급되는 시스템 헤더를 무시하고 다음을 include

  ```c
  #include <winsock2.h>
  #include <ws2tcpip.h>
  ```

- `winsock2`: Windows socket library의 "새로운" 버전
- `windows.h`를 include하면 `winsock.h`를 불러오기 때문에 `winsock2`와 충돌이 발생한다.
- `windows.h`를 include해야 한다면...

  ```c
  #define WIN32_LEAN_AND_MEAN

  #include <windows.h>
  #include <winsock2.h>
  ```

- 소켓 라이브러리를 이용해 무언가 하기 전에 `WSAStartup()`을 호출해야 한다.
  - Winsock 버전을 확인해야 한다.

  ```c
  #include <winsock2.h>

  {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
      fprintf(stderr, "WSAStartup failed.\n");
      exit(1);
    }

    if (LOBYTE(wsaData.wVersion) != 2 ||
        HIBYTE(wsaData.wVersion) != 2)
    {
        fprintf(stderr,"Versiion 2.2 of Winsock is not available.\n");
        WSACleanup();
        exit(2);
    }
  }
  ```

- 이외에도 참고할 사항이 여럿 있음

</details>

1. [What is a socket?](./01-socket.md)
2. [IP Addresses, `struct`s, and Data Munging](./02-ip.md)
3. [Jumping from IPv4 to IPv6](./03-jump.md)
4. [System Calls or Bust](./04-call.md)
5. [Client-Server Background](./05-background.md)
6. [Slightly Advanced Techniques](./06-advanced.md)
