static const char *PACKAGE_VERSION = "0.1";
static const char *PACKAGE = "Airpanel";
static const char *SOCKET_PATH;
extern struct DisplayProperties DISPLAY_PROPERTIES;

#include <algorithm>
#include <cctype>
#include <ctype.h>
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
  fprintf(stderr, "Usage: %s [-s SOCKET_PATH | -a refresh] [OPTIONS]\n",
          __progname);
  fprintf(stderr, "Version: %s\n", PACKAGE_VERSION);
  fprintf(stderr, "\n");
  fprintf(stderr, "Options available in both socket and CLI modes:\n");
  fprintf(stderr, " -h, --help                  display help and exit\n");
  fprintf(stderr, " -v, --version               print version and exit\n");
  fprintf(stderr, " -V, --verbose               switch on verbose logging\n");
  fprintf(stderr,
          " -W, --width WIDTH           set the display's native width\n");
  fprintf(stderr,
          " -H, --height HEIGHT         set the display's native height\n");
  fprintf(stderr, " -o, --orientation DEGREES   set display orientation to 0, "
                  "90, 180 or 270\n");
  fprintf(stderr,
          " -p, --processor PROCESSOR   set processor to BCM2835 or IT8951\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options available in socket mode:\n");
  fprintf(stderr,
          " -s, --socket SOCKET_PATH,   listen on a specified socket\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options available in CLI mode:\n");
  fprintf(stderr, " -a, --action refresh        display an image and exit\n");
  fprintf(stderr, " -i, --image IMG_PATH        image to display\n");
  fprintf(stderr,
          " -x, --offset-x OFFSET_PX    set the image left offset in px\n");
  fprintf(stderr,
          " -y, --offset-y OFFSET_PX    set the image top offset in px\n");
  fprintf(stderr, "\n");
}

inline double get_time() {
  struct timeval t;
  struct timezone tzp;
  gettimeofday(&t, &tzp);
  return t.tv_sec + t.tv_usec * 1e-6 * 1000;
}

int main(int argc, char *argv[]) {
  plog::init(plog::debug, &colorConsoleAppender);

  Action cli_action;
  cli_action.type = "cli";

  int ch;

  /* TODO:3001 If you want to add more options, add them here. */
  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"version", no_argument, 0, 'V'},
      {"verbose", no_argument, 0, 'v'},
      {"action", required_argument, 0, 'a'},
      {"socket", required_argument, 0, 's'},
      {"width", required_argument, 0, 'W'},
      {"height", required_argument, 0, 'H'},
      {"offset-x", required_argument, 0, 'x'},
      {"offset-y", required_argument, 0, 'y'},
      {"bpp", required_argument, 0, 'b'},
      {"processor", required_argument, 0, 'p'},
      {"orientation", required_argument, 0, 'o'},
      {"image", required_argument, 0, 'i'},
      {0, 0, 0, 0}};

  char *endptr;
  string optarg_string;

  while ((ch = getopt_long(argc, argv, "hVva:W:H:c:p:o:i:s:x:y:b:",
                           long_options, 0)) != -1) {

    if (ch == -1) {
      break;
    }

    switch (ch) {

    case 'h': {
      usage();
      exit(0);
    }

    case 'V': {
      fprintf(stdout, "%s %s\n", PACKAGE, PACKAGE_VERSION);
      exit(0);
    }

    case 'v': {
      LOG_INFO << "Enabled verbose logging mode";
      plog::get()->setMaxSeverity(plog::verbose);
      break;
    }

    case 'o': {
      int valid_orientations[] = {0, 90, 180, 270};
      long int orientation = strtol(optarg, &endptr, 0);
      if (!*endptr && std::find(std::begin(valid_orientations),
                                std::end(valid_orientations),
                                orientation) != std::end(valid_orientations)) {
        DISPLAY_PROPERTIES.orientation_specified = true;
        DISPLAY_PROPERTIES.orientation = orientation;
      } else {
        LOG_ERROR << "Supported display orientations are 0, 90, 180 and 270. '"
                  << optarg << "' isn't possible.";
        exit(1);
      }
      break;
    }

    case 's': {
      SOCKET_PATH = optarg;
      break;
    }

    case 'W': {
      long int width = strtol(optarg, &endptr, 0);
      if (!*endptr) {
        DISPLAY_PROPERTIES.width = width;
      }
      break;
    }

    case 'H': {
      long int height = strtol(optarg, &endptr, 0);
      if (!*endptr) {
        DISPLAY_PROPERTIES.height = height;
      }
      break;
    }

    case 'x': {
      long int x = strtol(optarg, &endptr, 0);
      if (!*endptr) {
        cli_action.offset_x = x;
      }
      break;
    }

    case 'y': {
      long int y = strtol(optarg, &endptr, 0);
      if (!*endptr) {
        cli_action.offset_y = y;
      }
      break;
    }

    // This is a private option, as changing the processor will change the color
    // mode automatically
    case 'b': {
      long int bpp = strtol(optarg, &endptr, 0);
      if (!*endptr) {
        switch (bpp) {
        case 8:
          DISPLAY_PROPERTIES.color_mode = 8;
          break;
        case 1:
          DISPLAY_PROPERTIES.color_mode = 1;
          break;
        default:
          LOG_ERROR << "Supported bits per pixel are 1 and 8.";
          exit(1);
        }
      } else {
        LOG_ERROR << "Supported bits per pixel are 1 and 8.";
        exit(1);
      }
      break;
    }

    case 'p': {
      optarg_string.assign(optarg);
      string valid_processors[] = {"BCM2835", "IT8951"};
      std::transform(optarg_string.begin(), optarg_string.end(),
                     optarg_string.begin(), ::toupper);

      if (std::find(std::begin(valid_processors), std::end(valid_processors),
                    optarg_string) != std::end(valid_processors)) {
        DISPLAY_PROPERTIES.processor = optarg_string;
      } else {
        LOG_ERROR << "Supported processors are BCM2835 and IT8951. '" << optarg
                  << "' isn't available.";
        exit(1);
      }

      break;
    }

    case 'a': {
      optarg_string.assign(optarg);
      string valid_actions[] = {"refresh"};
      std::transform(optarg_string.begin(), optarg_string.end(),
                     optarg_string.begin(), ::tolower);

      if (std::find(std::begin(valid_actions), std::end(valid_actions),
                    optarg_string) != std::end(valid_actions)) {
        cli_action.action = optarg_string;
      } else {
        LOG_ERROR << "The only supported action is 'refresh'. You requested '"
                  << optarg << "'.";
        exit(1);
      }
      break;
    }

    case 'i': {
      cli_action.image_filename = optarg;
      break;
    }

    default: {
      fprintf(stderr, "unknown option `%c'\n", ch);
      usage();
      exit(1);
    }
    }
  }

  if (!DISPLAY_PROPERTIES.width)
    DISPLAY_PROPERTIES.width = 640;

  if (!DISPLAY_PROPERTIES.height)
    DISPLAY_PROPERTIES.height = 384;

  if (DISPLAY_PROPERTIES.processor.empty())
    DISPLAY_PROPERTIES.processor = BCM2835;

  if (!DISPLAY_PROPERTIES.color_mode) {
    if (DISPLAY_PROPERTIES.processor == BCM2835) {
      DISPLAY_PROPERTIES.color_mode = COLOR_MODE_1BPP;
    } else {
      DISPLAY_PROPERTIES.color_mode = COLOR_MODE_8BPP;
    }
  }

  string bpp_string;
  if (DISPLAY_PROPERTIES.color_mode == COLOR_MODE_8BPP) {
    bpp_string = "(8 bits per pixel)";
  } else {
    bpp_string = "(1 bit per pixel)";
  }

  string orientation_string;
  if (DISPLAY_PROPERTIES.orientation_specified) {
    orientation_string =
        ", orientation: " + to_string(DISPLAY_PROPERTIES.orientation) + "°";
  } else {
    orientation_string = ", auto orientation";
  }

  LOG_INFO << DISPLAY_PROPERTIES.width << "×" << DISPLAY_PROPERTIES.height
           << ", " << DISPLAY_PROPERTIES.processor << " " << bpp_string
           << orientation_string;

  if (cli_action.action_is_refresh()) {
    if (SOCKET_PATH) {
      LOG_ERROR << "You specified a socket address to listen on but also an "
                   "action to perform. Please choose just one.";
      exit(1);
    }
    double time_start = get_time();

    if (!cli_action.has_image_filename()) {
      LOG_ERROR << "You must pass the path of an image to display using the -i "
                   "or --image option.";
      exit(1);
    }

    try {
      process_action(cli_action);
    } catch (exception &e) {
      LOG_ERROR << e.what();
      exit(1);
    }

    double time_end = get_time();
    char timeTaken[20];
    sprintf(timeTaken, "Took %.2f ms", (time_end - time_start));
    LOG_INFO << timeTaken;
    exit(0);
  }

  // not in CLI mode
  if (cli_action.has_image_filename()) {
    LOG_ERROR
        << "When listening on a socket, you must write a JSON message with "
           "the action and image filename to the socket instead of providing "
           "it as a command line option. A valid message written to "
        << SOCKET_PATH << " would be:\n"
        << R"({
  "type": "message",
  "data": {
    "action": "refresh",
    "image": ")"
        << cli_action.image_filename << R"("
  }
})";
    exit(1);
  }

  if (!SOCKET_PATH) {
    SOCKET_PATH = "/tmp/airpanel";
  }

  LOG_INFO << "Listening on socket " << SOCKET_PATH;

  /*
   * We will receive a JSON encoded message on the UNIX socket, in this
   * format:
   * {"type":"message","data":{"action":"refresh","image":"/path/to/the/image.png"}}
   */

  struct sockaddr_un addr;
  char buf[1024];
  int fd, cl, rc;

  // if (argc > 1)
  //  SOCKET_PATH = argv[1];

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
    LOG_ERROR << "Bind error. Maybe try `sudo`?";
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
      // Ensure the buffer passed to cJSON is null terminated, so the strlen
      // it calls doesn't suffer
      buf[rc - 1] = '\0'; // chops off the last \n too
      LOG_INFO << "Received message: " << buf;
      double time_start = get_time();

      try {
        Action action = parse_message(buf);
        process_action(action);
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
