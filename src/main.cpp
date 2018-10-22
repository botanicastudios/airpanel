static const char *PACKAGE_VERSION = "0.1";
static const char *PACKAGE = "Wollemi";
static const char *SOCKET_PATH = "/tmp/wollemi";

#include <getopt.h>

#include <vector>

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <sys/time.h>

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
  int debug = 1;
  int verbose = 1;
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
      debug++;
      break;
    }
    // fprintf(stderr, "unknown option `%c'\n", ch);
    // usage();
    // exit(1);
  }

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
    perror("socket error");
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
    perror("bind error");
    exit(-1);
  }

  if (listen(fd, 5) == -1) {
    perror("listen error");
    exit(-1);
  }

  while (1) {
    if ((cl = accept(fd, NULL, NULL)) == -1) {
      perror("accept error");
      continue;
    }

    while ((rc = static_cast<int>(read(cl, buf, sizeof(buf)))) > 0) {
      // Ensure the buffer passed to cJSON is null terminated, so the strlen it
      // calls doesn't suffer
      buf[rc] = '\0';
      printf("%.*s\n", rc, buf);

      double time_start = get_time();
      process_message(buf, debug, verbose);
      double time_end = get_time();
      printf("Took %.2f ms\n", (time_end - time_start));
    }
    if (rc == -1) {
      perror("read");
      exit(-1);
    } else if (rc == 0) {
      printf("EOF\n");
      close(cl);
    }
  }

  return EXIT_SUCCESS;
}
