#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#define STDIN 0

using namespace std;

int main(void) {
  struct timeval tv;
  fd_set readfds;

  tv.tv_sec = 2;
  tv.tv_usec = 500000;

  FD_ZERO(&readfds);
  FD_SET(STDIN, &readfds);

  select(STDIN + 1, &readfds, NULL, NULL, &tv);

  if (FD_ISSET(STDIN, &readfds)) {
    cout << "A key was pressed!" << endl;
  } else {
    cout << "Timed out." << endl;
  }

  return 0;
}