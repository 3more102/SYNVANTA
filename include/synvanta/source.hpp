#pragma once
#include <cstddef>
#include <string>
namespace synvanta {
struct SourceLocation {
    std::string file;
    std::size_t offset{0};
    std::size_t line{1};
    std::size_t column{1};
};
} // namespace synvanta
