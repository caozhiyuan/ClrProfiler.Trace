#include "logging.h"
#include <iostream>

namespace trace {

    void Log(const std::string& str) {
        std::cout << str.c_str() << "\n";
    }

}  // namespace trace
