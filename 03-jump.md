# IPv4에서 IPv6으로 넘어가기

1. 직접 구조체를 패킹하는 대신 [`getaddrinfo()`](./02-ip.md/#구조체)를 사용해 `struct sockaddr`의 정보를 모두 가져온다. 이렇게 하면 IP 버전에 구애받지 않는 상태를 유지하게 되고, 이후의 많은 단계를 제거하게 된다.
2. IP 버전에 관련되어 하드코딩한 모든 부분을 헬퍼 함수로 감싼다.
3. `AF_INET`을 `AF_INET6`으로 변경한다.
4. `PF_INET`을 `PF_INET6`으로 변경한다.
5. 할당된 `INADDR_ANY`을 `in6addr_any`로 변경한다.

    ```c
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    sa.sin_addr.s_addr = INADDR_ANY; // use my IPv4 address
    sa6.sin6_addr = in6addr_any;     // use my IPv6 address
    ```

    또한, `IN6ADDR_ANY_INIT` 값은 다음과 같이 `struct in6_addr`이 선언될 때 생성자로 사용될 수 있다.

    ```c
    struct in6_addr ia6 = IN6ADDR_ANY_INIT;
    ```

6. `struct sockaddr_in` 대신 `struct sockaddr_in6`을 사용한다. 이때 필드에 적절하게 "6"을 추가한다. `sin6_zero` 필드는 존재하지 않는다.
7. `struct in_addr` 대신 `struct in6_addr`을 사용한다. 이때 필드에 적절하게 "6"을 추가한다.
8. `inet_aton()` 또는 `inet_addr()` 대신 `inet_pton()`을 사용한다.
9. `inet_nota()` 대신 `inet_ntop()`을 사용한다.
10. `gethostbyname()` 대신 `getaddrinfo()`를 사용한다.
11. (IPv6에서도 `gethostbyaddr()`을 사용할 수 있지만) `gethostbyaddr()` 대신 `getnameinfo()`를 사용한다.
12. `INADDR_BROADCAST`는 더이상 동작하지 않는다. 대신 IPv6 멀티캐스트를 사용한다.
