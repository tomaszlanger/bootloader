#include "bootloader.h"
#include <string>
#include <mutex>
#include <chrono>
#include <thread>

namespace bootloader {


bootloader::bootloader() : sp(std::string(""), 38400, serial::Timeout::simpleTimeout(50)) {
}

bootloader::~bootloader() {
	close();
}

}  // namespace bootloader