#pragma once

#include <string>
#include <string_view>

std::wstring Utf8ToWide(std::string_view s);
std::string WideToUtf8(std::wstring_view ws);
std::wstring GetEnvWide(const wchar_t* name);

