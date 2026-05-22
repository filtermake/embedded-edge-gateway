#include "Logger.h"

int main() {
    LOG_INFO("gateway %s starting...", "v0.1.0");
    LOG_WARN("this is a warning");
    LOG_ERROR("this is an error");
    return 0;
}
