#pragma once

#include "RE/Fallout.h"
#include "F4SE/F4SE.h"

#include <Windows.h>

#ifdef ERROR
#undef ERROR
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>

#define DLLEXPORT __declspec(dllexport)

namespace logger = F4SE::log;

using namespace std::string_view_literals;
