#include "log_formatter.h"
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Log.h>

static plog::ColorConsoleAppender<plog::AirpanelTxtFormatterUtcTime>
    colorConsoleAppender;
