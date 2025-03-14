#pragma once
#include <string>

static bool isValidCString(const char* str)
{
    // Check if the string is not NULL and has at least one character
    return str != nullptr && strlen(str) > 0 && str[strlen(str)] == '\0';
}
