#include "http_agent_client.h"
#include "strings.h"

#include <Windows.h>
#include <CommCtrl.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace
{
constexpr int kIdChatLog = 1001;
constexpr int kIdInput = 1002;
constexpr int kIdSend = 1003;
constexpr int kIdStatus = 1004;

constexpr UINT kMsgAgentReply = WM_APP + 1;

struct UiState
{
  HWND hwnd = nullptr;
  HWND chatLog = nullptr;
  HWND input = nullptr;
  HWND sendBtn = nullptr;
  HWND status = nullptr;

  std::mutex mu;
  std::string pendingReplyUtf8;
  std::string pendingErrorUtf8;
  std::atomic_bool busy{ false };

  std::string baseUrlUtf8;
};

void AppendText(HWND edit, const std::wstring& text)
{
  int len = GetWindowTextLengthW(edit);
  SendMessageW(edit, EM_SETSEL, len, len);
  SendMessageW(edit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
}

std::wstring GetWindowTextString(HWND hwnd)
{
  int len = GetWindowTextLengthW(hwnd);
  std::wstring out;
  out.resize(static_cast<size_t>(len));
  if (len > 0)
    GetWindowTextW(hwnd, out.data(), len + 1);
  return out;
}

std::string ResolveBaseUrlUtf8()
{
  std::wstring v = GetEnvWide(L"AGENT_BASE_URL");
  if (v.empty())
    v = GetEnvWide(L"AI_AGENT_URL");
  if (v.empty())
    v = L"http://127.0.0.1:8787";
  return WideToUtf8(v);
}

void SetBusy(UiState* s, bool busy, const wchar_t* statusText)
{
  s->busy.store(busy);
  EnableWindow(s->sendBtn, busy ? FALSE : TRUE);
  EnableWindow(s->input, busy ? FALSE : TRUE);
  SetWindowTextW(s->status, statusText ? statusText : L"");
}

void StartChatRequest(UiState* s)
{
  if (s->busy.load())
    return;

  std::wstring inputW = GetWindowTextString(s->input);
  if (inputW.empty())
    return;

  std::string messageUtf8 = WideToUtf8(inputW);

  SetWindowTextW(s->input, L"");
  AppendText(s->chatLog, L"You: ");
  AppendText(s->chatLog, Utf8ToWide(messageUtf8));
  AppendText(s->chatLog, L"\r\n\r\n");

  SetBusy(s, true, L"Sending...");

  std::string baseUrl = s->baseUrlUtf8;
  HWND hwnd = s->hwnd;

  std::thread([s, hwnd, baseUrl, msg = std::move(messageUtf8)]() mutable {
    try
    {
      HttpAgentClient client(baseUrl);
      std::string reply = client.Chat(msg);
      {
        std::lock_guard<std::mutex> lock(s->mu);
        s->pendingReplyUtf8 = std::move(reply);
        s->pendingErrorUtf8.clear();
      }
    }
    catch (const std::exception& e)
    {
      std::lock_guard<std::mutex> lock(s->mu);
      s->pendingReplyUtf8.clear();
      s->pendingErrorUtf8 = e.what();
    }
    PostMessageW(hwnd, kMsgAgentReply, 0, 0);
  }).detach();
}

void RefreshLayout(UiState* s)
{
  RECT rc{};
  GetClientRect(s->hwnd, &rc);

  const int pad = 10;
  const int statusH = 20;
  const int inputH = 28;
  const int btnW = 90;

  int w = rc.right - rc.left;
  int h = rc.bottom - rc.top;

  int bottom = h - pad;

  // Status
  MoveWindow(s->status, pad, bottom - statusH, w - 2 * pad, statusH, TRUE);
  bottom -= statusH + pad;

  // Input row
  MoveWindow(s->sendBtn, w - pad - btnW, bottom - inputH, btnW, inputH, TRUE);
  MoveWindow(s->input, pad, bottom - inputH, w - 3 * pad - btnW, inputH, TRUE);
  bottom -= inputH + pad;

  // Chat log
  MoveWindow(s->chatLog, pad, pad, w - 2 * pad, bottom - pad, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  UiState* s = reinterpret_cast<UiState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg)
  {
  case WM_CREATE:
  {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    s = reinterpret_cast<UiState*>(cs->lpCreateParams);
    s->hwnd = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));

    s->chatLog = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
      0, 0, 0, 0,
      hwnd, reinterpret_cast<HMENU>(kIdChatLog), GetModuleHandleW(nullptr), nullptr);

    s->input = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      0, 0, 0, 0,
      hwnd, reinterpret_cast<HMENU>(kIdInput), GetModuleHandleW(nullptr), nullptr);

    s->sendBtn = CreateWindowExW(
      0, L"BUTTON", L"Send",
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      0, 0, 0, 0,
      hwnd, reinterpret_cast<HMENU>(kIdSend), GetModuleHandleW(nullptr), nullptr);

    s->status = CreateWindowExW(
      0, L"STATIC", L"",
      WS_CHILD | WS_VISIBLE,
      0, 0, 0, 0,
      hwnd, reinterpret_cast<HMENU>(kIdStatus), GetModuleHandleW(nullptr), nullptr);

    // Nice default font
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);
    SendMessageW(s->chatLog, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(s->input, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(s->sendBtn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(s->status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    RefreshLayout(s);

    AppendText(s->chatLog, L"AiAgentUiCpp started.\r\n");
    AppendText(s->chatLog, L"Base URL: ");
    AppendText(s->chatLog, Utf8ToWide(s->baseUrlUtf8));
    AppendText(s->chatLog, L"\r\n\r\n");

    SetBusy(s, false, L"Ready");
    SetFocus(s->input);
    return 0;
  }
  case WM_SIZE:
    if (s)
      RefreshLayout(s);
    return 0;

  case WM_COMMAND:
    if (!s)
      break;

    if (LOWORD(wParam) == kIdSend && HIWORD(wParam) == BN_CLICKED)
    {
      StartChatRequest(s);
      return 0;
    }
    if (LOWORD(wParam) == kIdInput && HIWORD(wParam) == EN_UPDATE)
    {
      // no-op (reserved)
      return 0;
    }
    break;

  case WM_KEYDOWN:
    if (s && wParam == VK_RETURN)
    {
      HWND focus = GetFocus();
      if (focus == s->input && !s->busy.load())
      {
        StartChatRequest(s);
        return 0;
      }
    }
    break;

  case kMsgAgentReply:
    if (!s)
      break;

    {
      std::string reply, err;
      {
        std::lock_guard<std::mutex> lock(s->mu);
        reply = s->pendingReplyUtf8;
        err = s->pendingErrorUtf8;
        s->pendingReplyUtf8.clear();
        s->pendingErrorUtf8.clear();
      }

      if (!err.empty())
      {
        AppendText(s->chatLog, L"Error: ");
        AppendText(s->chatLog, Utf8ToWide(err));
        AppendText(s->chatLog, L"\r\n\r\n");
        SetBusy(s, false, L"Error");
      }
      else
      {
        AppendText(s->chatLog, L"Agent: ");
        AppendText(s->chatLog, Utf8ToWide(reply));
        AppendText(s->chatLog, L"\r\n\r\n");
        SetBusy(s, false, L"Ready");
      }
    }
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
  INITCOMMONCONTROLSEX icc{};
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&icc);

  UiState state;
  state.baseUrlUtf8 = ResolveBaseUrlUtf8();

  const wchar_t kClassName[] = L"AiAgentUiCppMainWindow";
  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = kClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(
    0,
    kClassName,
    L"AiAgentUiCpp",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
    nullptr, nullptr, hInstance, &state);

  if (!hwnd)
    return 1;

  ShowWindow(hwnd, nCmdShow);

  MSG m{};
  while (GetMessageW(&m, nullptr, 0, 0))
  {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  return 0;
}

