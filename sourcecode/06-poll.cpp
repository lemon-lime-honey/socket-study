#include <poll.h>

#include <iostream>

using namespace std;

int main(void) {
  struct pollfd pfds[1];

  pfds[0].fd = 0;
  pfds[0].events = POLLIN;

  cout << "Hit RETURN or wait 2.5 seconds for timeout" << endl;

  int num_events = poll(pfds, 1, 2500);

  if (num_events == 0) {
    cout << "Poll timed out!" << endl;
  } else {
    int pollin_happened = pfds[0].revents & POLLIN;

    if (pollin_happened) {
      cout << "File descriptor " << pfds[0].fd << " is ready to read" << endl;
    } else {
      cout << "Unexpected event occurred: " << pfds[0].revents << endl;
    }
  }

  return 0;
}