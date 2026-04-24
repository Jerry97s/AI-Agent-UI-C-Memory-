#include "strings.h"

#include <Windows.h>

std::wstring Utf8ToWide(std::string_view s)
{
  if (s.empty())
    return {};

  int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (len <= 0)
    return {};

  std::wstring out;
  out.resize(static_cast<size_t>(len));
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
  return out;
}

std::string WideToUtf8(std::wstring_view ws)
{
  if (ws.empty())
    return {};

  int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return {};

  std::string out;
  out.resize(static_cast<size_t>(len));
  WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), len, nullptr, nullptr);
  return out;
}

std::wstring GetEnvWide(const wchar_t* name)
{
  if (!name)
    return {};

  DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
  if (needed == 0)
    return {};

  std::wstring value;
  value.resize(needed);
  DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
  if (written == 0)
    return {};

  // Remove trailing null.
  if (!value.empty() && value.back() == L'\0')
    value.pop_back();
  return value;
}

