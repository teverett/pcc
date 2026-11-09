#pragma once

#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

ObjectFile readHostedCoff(const std::string &bytes);

}
