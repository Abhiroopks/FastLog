#include "fastlog.h" // Included automatically via target_include_directories
#include <iostream>

const std::string FILE_NAME = "log.log";
const bool STD_OUT = true;

int main()
{
    FastLog &logger = FastLog::getInstance(FILE_NAME, STD_OUT);

    logger.logInfo("hi");

    return 0;
}
