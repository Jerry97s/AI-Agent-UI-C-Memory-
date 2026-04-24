#include "http_agent_client.h"

#include "strings.h"

#include <Windows.h>
#include <winhttp.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
std::string JsonEscape(std::string_view s)
{
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s)
  {
    switch (c)
    {
    case '\\': out += "\\\\"; break;
    case '"': out += "\\\""; break;
    case '\b': out += "\\b"; break;
    case '\f': out += "\\f"; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    default:
      if (static_cast<unsigned char>(c) < 0x20)
      {
        char buf[7];
        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
        out += buf;
      }
      else
      {
        out += c;
      }
      break;
    }
  }
  return out;
}

std::string BuildChatRequestBody(std::string_view message)
{
  return std::string("{\"message\":\"") + JsonEscape(message) + "\"}";
}

// Minimal extraction for {"reply":"..."}.
// Assumes server returns compact/simple JSON as in docs/API.md.
std::string ExtractJsonStringField(std::string_view jsonText, std::string_view key)
{
  // Find "key"
  std::string needle = std::string("\"") + std::string(key) + "\"";
  size_t pos = jsonText.find(needle);
  if (pos == std::string_view::npos)
    return {};
  pos = jsonText.find(':', pos + needle.size());
  if (pos == std::string_view::npos)
    return {};
  pos++;
  while (pos < jsonText.size() && (jsonText[pos] == ' ' || jsonText[pos] == '\n' || jsonText[pos] == '\r' || jsonText[pos] == '\t'))
    pos++;
  if (pos >= jsonText.size() || jsonText[pos] != '"')
    return {};
  pos++; // after first quote

  std::string out;
  for (; pos < jsonText.size(); ++pos)
  {
    char c = jsonText[pos];
    if (c == '"')
      break;
    if (c == '\\')
    {
      if (pos + 1 >= jsonText.size())
        break;
      char e = jsonText[++pos];
      switch (e)
      {
      case '"': out.push_back('"'); break;
      case '\\': out.push_back('\\'); break;
      case '/': out.push_back('/'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'n': out.push_back('\n'); break;
      case 'r': out.push_back('\r'); break;
      case 't': out.push_back('\t'); break;
      // \uXXXX is not needed for our server replies; ignore gracefully.
      default: out.push_back(e); break;
      }
    }
    else
    {
      out.push_back(c);
    }
  }
  return out;
}

struct ParsedBaseUrl
{
  std::wstring host;
  INTERNET_PORT port = 0;
  bool https = false;
  std::wstring base_path; // no trailing slash ("" or "/api")
};

ParsedBaseUrl ParseBaseUrl(std::string base_url_utf8)
{
  std::wstring url = Utf8ToWide(base_url_utf8);
  if (url.empty())
    throw std::runtime_error("base_url is empty");

  URL_COMPONENTS uc{};
  uc.dwStructSize = sizeof(uc);
  uc.dwSchemeLength = static_cast<DWORD>(-1);
  uc.dwHostNameLength = static_cast<DWORD>(-1);
  uc.dwUrlPathLength = static_cast<DWORD>(-1);

  if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
    throw std::runtime_error("invalid base_url");

  ParsedBaseUrl out;
  out.https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
  out.port = uc.nPort;

  if (uc.lpszHostName && uc.dwHostNameLength > 0)
    out.host.assign(uc.lpszHostName, uc.lpszHostName + uc.dwHostNameLength);

  if (uc.lpszUrlPath && uc.dwUrlPathLength > 0)
    out.base_path.assign(uc.lpszUrlPath, uc.lpszUrlPath + uc.dwUrlPathLength);

  // Normalize base_path: remove trailing slash, but keep "/" as empty.
  while (!out.base_path.empty() && out.base_path.back() == L'/')
    out.base_path.pop_back();
  if (out.base_path == L"/")
    out.base_path.clear();

  if (out.host.empty())
    throw std::runtime_error("base_url host is empty");
  if (out.port == 0)
    out.port = out.https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

  return out;
}

std::wstring JoinPath(std::wstring base, std::wstring_view suffix)
{
  // base is "" or "/x"; suffix is "/health" etc.
  if (base.empty())
    return std::wstring(suffix);
  if (suffix.empty())
    return base;
  return base + std::wstring(suffix);
}

std::string WinHttpRequestUtf8(
  const ParsedBaseUrl& base,
  std::wstring_view method,
  std::wstring_view path,
  std::string body_utf8,
  std::wstring_view content_type)
{
  HINTERNET hSession = WinHttpOpen(L"AiAgentUiCpp/0.1",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME,
    WINHTTP_NO_PROXY_BYPASS,
    0);
  if (!hSession)
    throw std::runtime_error("WinHttpOpen failed");

  DWORD timeoutMs = 30'000;
  WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

  HINTERNET hConnect = WinHttpConnect(hSession, base.host.c_str(), base.port, 0);
  if (!hConnect)
  {
    WinHttpCloseHandle(hSession);
    throw std::runtime_error("WinHttpConnect failed");
  }

  std::wstring fullPath = JoinPath(base.base_path, path);
  DWORD flags = base.https ? WINHTTP_FLAG_SECURE : 0;

  HINTERNET hRequest = WinHttpOpenRequest(
    hConnect,
    method.data(),
    fullPath.c_str(),
    nullptr,
    WINHTTP_NO_REFERER,
    WINHTTP_DEFAULT_ACCEPT_TYPES,
    flags);
  if (!hRequest)
  {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    throw std::runtime_error("WinHttpOpenRequest failed");
  }

  std::wstring headers = L"Accept: application/json\r\n";
  if (!content_type.empty())
  {
    headers += L"Content-Type: ";
    headers += content_type;
    headers += L"\r\n";
  }

  const void* bodyPtr = body_utf8.empty() ? WINHTTP_NO_REQUEST_DATA : body_utf8.data();
  DWORD bodyLen = body_utf8.empty() ? 0 : static_cast<DWORD>(body_utf8.size());

  BOOL ok = WinHttpSendRequest(
    hRequest,
    headers.c_str(),
    static_cast<DWORD>(headers.size()),
    const_cast<void*>(bodyPtr),
    bodyLen,
    bodyLen,
    0);
  if (!ok)
  {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    throw std::runtime_error("WinHttpSendRequest failed");
  }

  ok = WinHttpReceiveResponse(hRequest, nullptr);
  if (!ok)
  {
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    throw std::runtime_error("WinHttpReceiveResponse failed");
  }

  DWORD status = 0;
  DWORD statusSize = sizeof(status);
  WinHttpQueryHeaders(hRequest,
    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
    WINHTTP_HEADER_NAME_BY_INDEX,
    &status,
    &statusSize,
    WINHTTP_NO_HEADER_INDEX);

  std::string response;
  for (;;)
  {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &avail))
      break;
    if (avail == 0)
      break;

    size_t old = response.size();
    response.resize(old + avail);
    DWORD read = 0;
    if (!WinHttpReadData(hRequest, response.data() + old, avail, &read))
      break;
    response.resize(old + read);
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  if (status < 200 || status >= 300)
  {
    throw std::runtime_error("HTTP error: " + std::to_string(status) + " body=" + response);
  }

  return response;
}
} // namespace

HttpAgentClient::HttpAgentClient(std::string base_url)
  : base_url_(std::move(base_url))
{
}

AgentHealth HttpAgentClient::GetHealth() const
{
  ParsedBaseUrl base = ParseBaseUrl(base_url_);
  std::string resp = WinHttpRequestUtf8(base, L"GET", L"/health", {}, L"");

  AgentHealth h;
  h.status = ExtractJsonStringField(resp, "status");
  h.version = ExtractJsonStringField(resp, "version");
  h.mode = ExtractJsonStringField(resp, "mode");
  h.model = ExtractJsonStringField(resp, "model");
  return h;
}

std::string HttpAgentClient::Chat(std::string_view message) const
{
  ParsedBaseUrl base = ParseBaseUrl(base_url_);

  std::string body = BuildChatRequestBody(message);
  std::string resp = WinHttpRequestUtf8(base, L"POST", L"/chat", std::move(body), L"application/json");
  return ExtractJsonStringField(resp, "reply");
}

