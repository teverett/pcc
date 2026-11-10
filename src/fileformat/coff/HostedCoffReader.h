#pragma once

#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

ObjectFile readHostedCoff(const std::string &bytes);

}
