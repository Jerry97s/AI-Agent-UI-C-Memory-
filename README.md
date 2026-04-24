# AI Agent UI (C++)

`AI_Agent_UI`(WPF) 프로젝트의 **에이전트 HTTP API**(`GET /health`, `POST /chat`) 규격을 그대로 사용해서, Windows에서 실행되는 **C++ 네이티브 채팅 UI(MVP)** 를 제공합니다.

## 기능 (MVP)

- 에이전트 URL: 기본 `http://127.0.0.1:8787`  
  - 환경변수 `AGENT_BASE_URL` 또는 `AI_AGENT_URL` 로 변경 가능
- 채팅 전송: `POST /chat` (`{"message":"..."}`) → `{"reply":"..."}`
- UI: 채팅 로그 / 입력창 / Send 버튼 / 상태 표시

## 빌드 (CMake + Visual Studio)

Visual Studio에서 폴더 열기(Open Folder)로 CMake 프로젝트를 열고 빌드/실행합니다.

또는 개발자 PowerShell에서:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\AiAgentUiCpp.exe
```

## 에이전트 서버 실행 (참조 프로젝트)

`AI_Agent_UI\python\agent_server.py` 를 먼저 실행하세요.

