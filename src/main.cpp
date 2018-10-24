static const char *PACKAGE_VERSION = "0.1";
static const char *PACKAGE = "Wollemi";
static const char *SOCKET_PATH = "/tmp/wollemi";

#include <getopt.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <vector>

#include "core.h"
extern const char *__progname;

static void usage(void) {
  /* TODO:3002 Don't forget to update the usage block with the most
   * TODO:3002 important options. */
  fprintf(stderr, "Usage: %s [OPTIONS]\n", __progname);
  fprintf(stderr, "Version: %s\n", PACKAGE_VERSION);
  fprintf(stderr, "\n");
  fprintf(stderr, " -d, --debug        be more verbose.\n");
  fprintf(stderr, " -h, --help         display help and exit\n");
  fprintf(stderr, " -v, --version      print version and exit\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "see manual page %s (8) for more information\n", PACKAGE);
}

inline double get_time() {
  struct timeval t;
  struct timezone tzp;
  gettimeofday(&t, &tzp);
  return t.tv_sec + t.tv_usec * 1e-6 * 1000;
}

int main(int argc, char *argv[]) {
  plog::init(plog::debug, &colorConsoleAppender);

  int ch;

  /* TODO:3001 If you want to add more options, add them here. */
  static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                         {"version", no_argument, 0, 'v'},
                                         {"debug", no_argument, 0, 'x'},
                                         {0, 0, 0, 0}};
  // while (1) {
  // int option_index = 0;
  while ((ch = getopt_long(argc, argv, ":hvx", long_options, 0)) != -1) {
    printf("%c\n", ch);
    if (ch == -1)
      break;
    if (ch == 'h') {
      usage();
      exit(0);
    }
    if (ch == 'v') {
      fprintf(stdout, "%s %s\n", PACKAGE, PACKAGE_VERSION);
      exit(0);
    }
    if (ch == 'x') {
      printf("flag is x\n");
      break;
    }
    // fprintf(stderr, "unknown option `%c'\n", ch);
    // usage();
    // exit(1);
  }

  extern struct DisplayProperties DISPLAY_PROPERTIES;
  DISPLAY_PROPERTIES.width = 640;
  DISPLAY_PROPERTIES.height = 384;
  DISPLAY_PROPERTIES.color_mode = COLOR_MODE_1BPP;
  DISPLAY_PROPERTIES.processor = BCM2835;

  /*
   * We will receive a JSON encoded message on the UNIX socket, in this
   * format:
   * {"type":"message","data":{"action":"refresh","image":"/path/to/the/image.png"}}
   */

  struct sockaddr_un addr;
  char buf[1024];
  int fd, cl, rc;

  if (argc > 1)
    SOCKET_PATH = argv[1];

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    LOG_ERROR << "Socket error";
    exit(-1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (*SOCKET_PATH == '\0') {
    *addr.sun_path = '\0';
    strncpy(addr.sun_path + 1, SOCKET_PATH + 1, sizeof(addr.sun_path) - 2);
  } else {
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);
  }

  if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) ==
      -1) {
    LOG_ERROR << "Bind error";
    exit(-1);
  }

  if (listen(fd, 5) == -1) {
    LOG_ERROR << "Listen error";
    exit(-1);
  }

  while (1) {
    if ((cl = accept(fd, NULL, NULL)) == -1) {
      LOG_ERROR << "Accept error";
      continue;
    }

    while ((rc = static_cast<int>(read(cl, buf, sizeof(buf)))) > 0) {
      // Ensure the buffer passed to cJSON is null terminated, so the strlen it
      // calls doesn't suffer
      buf[rc - 1] = '\0'; // chops off the last \n too
      LOG_INFO << "Received message: " << buf;
      double time_start = get_time();

      try {
        Message message = parse_message(buf);
        process_message(message);
      } catch (exception &e) {
        LOG_ERROR << e.what();
      }

      double time_end = get_time();
      char timeTaken[20];
      sprintf(timeTaken, "Took %.2f ms", (time_end - time_start));
      LOG_INFO << timeTaken;
    }
    if (rc == -1) {
      LOG_ERROR << "Read error";
      exit(-1);
    } else if (rc == 0) {
      LOG_INFO << "EOF";
      close(cl);
    }
  }

  return EXIT_SUCCESS;
}
