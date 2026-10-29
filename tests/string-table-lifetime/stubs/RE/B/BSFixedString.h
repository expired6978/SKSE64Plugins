#pragma once
#include <string>
namespace RE {
class BSFixedString {
    std::string value;
public:
    BSFixedString(const char* text) : value(text) {}
    const char* c_str() const { return value.c_str(); }
};
}
