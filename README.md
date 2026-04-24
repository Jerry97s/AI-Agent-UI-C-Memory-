# AI Agent UI (C++)

Windows에서 실행되는 **C++(Win32) 네이티브 AI Agent 채팅 UI**입니다.  
참조 프로젝트 `AI_Agent_UI`(WPF)의 에이전트 HTTP API(`GET /health`, `POST /chat`) 규격을 그대로 사용합니다.

> 레퍼런스 스타일: [`AI-Fast-Server-Python-`](https://github.com/Jerry97s/AI-Fast-Server-Python-)

---

## 기능 (MVP)

- **에이전트 URL**: 기본 `http://127.0.0.1:8787`
  - 환경변수 `AGENT_BASE_URL` 또는 `AI_AGENT_URL`로 변경
- **채팅 전송**: `POST /chat` (`{"message":"..."}`) → `{"reply":"..."}`
- **UI**: 채팅 로그 / 입력창 / Send 버튼 / 상태(Ready/Sending/Error)

---

## 기술 스택

| 계층 | 기술 |
|---|---|
| UI | Win32 (CreateWindowEx / EDIT / BUTTON / STATIC) |
| HTTP | WinHTTP (`winhttp.dll`) |
| 빌드 | CMake (Visual Studio Open Folder 지원) |
| 인코딩 | UTF-8 ↔ UTF-16 변환(WinAPI) |

---

## 프로젝트 구조

```
AI_Agent_UI_C++/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp              # Win32 UI + 비동기 요청/응답
    ├── http_agent_client.h
    ├── http_agent_client.cpp # WinHTTP로 /health, /chat 호출
    ├── strings.h
    └── strings.cpp           # UTF-8/UTF-16, 환경변수
```

---

## 빌드 및 실행 (CMake + Visual Studio)

### 사전 조건

- Windows 10/11
- Visual Studio 2022 (C++ Desktop 개발 워크로드 권장)
- (선택) CMake가 PATH에 있으면 CLI 빌드 가능

### Visual Studio

Visual Studio에서 폴더 열기(Open Folder)로 `AI_Agent_UI_C++`를 열고 빌드/실행합니다.

### CLI (CMake 설치된 경우)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\AiAgentUiCpp.exe
```

---

## 에이전트 서버 실행 (참조 프로젝트)

`AI_Agent_UI\python\agent_server.py`를 먼저 실행하세요. (기본 `http://127.0.0.1:8787`)

---

## 에이전트 HTTP API 요약

### `GET /health`

성공 응답(예):

```json
{
  "status": "ok",
  "version": "1.1.0",
  "mode": "demo",
  "model": null
}
```

### `POST /chat`

요청:

```json
{ "message": "사용자 메시지" }
```

응답:

```json
{ "reply": "에이전트 응답 텍스트" }
```

---

## 아키텍처

```
┌──────────────────────────────────────┐
│           사용자 / Win32 UI           │
│  - 채팅 로그 / 입력 / 버튼 / 상태     │
└──────────────┬───────────────────────┘
               │ HTTP (WinHTTP)
               │  - GET /health
               │  - POST /chat
               ▼
┌──────────────────────────────────────┐
│     Python Agent Server (FastAPI)     │
│  - demo(에코) 또는 LLM 모드           │
└──────────────────────────────────────┘
```

---

## 프로젝트 분석 (요약)

### 강점

- **의존성 최소화**: Win32 + WinHTTP로 외부 SDK 없이 동작
- **API 호환성**: `AI_Agent_UI`의 `/health`, `/chat` 규격을 그대로 사용
- **확장 포인트 명확**: UI와 통신을 `HttpAgentClient`로 분리

### 리스크/제약

- **JSON 파싱 단순화**: MVP에서는 `{"reply":"..."}` 형태에 최적화된 최소 파서(문자열 추출)만 사용
- **UI 기능 제한**: WPF 버전의 탭/핀/드래그앤드롭/트레이/핫키/영속화는 아직 미구현
- **원격 접속 보안**: 로컬 루프백 기준. 원격 노출 시 TLS/인증이 필요

### 다음 작업(추천)

- **정식 JSON 라이브러리 도입**: vcpkg/conan로 `nlohmann-json` 또는 `rapidjson` 연결
- **스트리밍 응답**: SSE 또는 chunked response로 토큰 단위 UX
- **대화 영속화/로그**: `state.json`, JSONL 로그(참조 프로젝트 방식)
- **UI 고도화**: 대화 탭/핀/파일 업로드/트레이/전역 단축키


