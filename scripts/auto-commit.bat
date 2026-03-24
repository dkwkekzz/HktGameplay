@echo off
chcp 65001 >nul 2>&1
setlocal EnableDelayedExpansion

:: auto-commit.bat - AI agent가 git diff를 분석하여 커밋 메시지를 생성, commit & push
:: Usage: scripts\auto-commit.bat [branch-name]
::
:: 환경변수:
::   ANTHROPIC_API_KEY  - Claude API 키 (필수)
::   COMMIT_MODEL       - 사용할 모델 (기본값: claude-haiku-4-5-20251001)
::   NO_AI              - "1"로 설정하면 AI 없이 fallback 메시지 사용

:: repo root로 이동
for /f "delims=" %%i in ('git rev-parse --show-toplevel') do set "REPO_ROOT=%%i"
cd /d "%REPO_ROOT%"

:: 브랜치 결정
if "%~1"=="" (
    for /f "delims=" %%b in ('git branch --show-current') do set "BRANCH=%%b"
) else (
    set "BRANCH=%~1"
)

if not defined COMMIT_MODEL set "COMMIT_MODEL=claude-haiku-4-5-20251001"

echo === HktGameplay Auto Commit ^& Push (AI) ===
echo Branch: %BRANCH%
echo.

:: 변경점 확인
set "HAS_CHANGES=0"
for /f "delims=" %%f in ('git diff --cached --name-only 2^>nul') do set "HAS_CHANGES=1"
for /f "delims=" %%f in ('git diff --name-only 2^>nul') do set "HAS_CHANGES=1"
for /f "delims=" %%f in ('git ls-files --others --exclude-standard 2^>nul') do set "HAS_CHANGES=1"

if "%HAS_CHANGES%"=="0" (
    echo 변경점이 없습니다.
    exit /b 0
)

echo [변경점]
git status -s
echo.

:: diff를 임시 파일에 저장
set "DIFF_FILE=%TEMP%\hkt_git_diff.txt"
set "RESPONSE_FILE=%TEMP%\hkt_ai_response.json"
set "REQUEST_FILE=%TEMP%\hkt_ai_request.json"

(
    echo [File Stats]
    git diff --cached --stat 2>nul
    git diff --stat 2>nul
    echo.
    echo [Diff Detail]
    git diff --cached -U3 2>nul
    git diff -U3 2>nul
    echo.
    echo [Recent Commits]
    git log --oneline -5 2>nul
) > "%DIFF_FILE%"

:: AI 메시지 생성 시도
set "COMMIT_MSG="
set "USED_AI=0"

if "%NO_AI%"=="1" goto :fallback
if not defined ANTHROPIC_API_KEY goto :fallback

:: curl과 jq 확인
where curl >nul 2>&1 || goto :fallback
where jq >nul 2>&1 || (
    echo jq가 설치되어 있지 않습니다. AI 메시지 생성을 건너뜁니다.
    goto :fallback
)

echo [AI가 커밋 메시지를 분석 중...]

:: diff 내용을 읽어서 JSON escape (PowerShell 사용)
for /f "usebackq delims=" %%a in (`powershell -NoProfile -Command "$c = Get-Content '%DIFF_FILE%' -Raw -ErrorAction SilentlyContinue; if($c.Length -gt 4000){$c=$c.Substring(0,4000)}; $c | ConvertTo-Json"`) do set "ESCAPED_DIFF=%%a"

:: 요청 JSON 생성
powershell -NoProfile -Command ^
  "$diff = Get-Content '%DIFF_FILE%' -Raw -ErrorAction SilentlyContinue; " ^
  "if($diff.Length -gt 4000){$diff=$diff.Substring(0,4000)}; " ^
  "$prompt = 'You are a commit message generator for an Unreal Engine 5 MMORPG project (HktGameplay). Analyze the git diff below and write a concise, conventional commit message.' + [char]10 + [char]10 + 'Rules:' + [char]10 + '- Format: <type>: <description>' + [char]10 + '- Types: feat, fix, refactor, docs, chore, test, style' + [char]10 + '- Description in English, max 72 chars' + [char]10 + '- Focus on WHAT changed and WHY, not HOW' + [char]10 + [char]10 + 'Git diff context:' + [char]10 + $diff + [char]10 + [char]10 + 'Respond with ONLY the commit message, nothing else.'; " ^
  "$body = @{model='%COMMIT_MODEL%'; max_tokens=256; messages=@(@{role='user'; content=$prompt})} | ConvertTo-Json -Depth 5; " ^
  "[System.IO.File]::WriteAllText('%REQUEST_FILE%', $body, [System.Text.Encoding]::UTF8)"

curl -s --max-time 15 ^
    -H "Content-Type: application/json" ^
    -H "x-api-key: %ANTHROPIC_API_KEY%" ^
    -H "anthropic-version: 2023-06-01" ^
    -d @"%REQUEST_FILE%" ^
    "https://api.anthropic.com/v1/messages" > "%RESPONSE_FILE%" 2>nul

:: 응답에서 메시지 추출
for /f "usebackq delims=" %%m in (`jq -r ".content[0].text // empty" "%RESPONSE_FILE%" 2^>nul`) do (
    set "COMMIT_MSG=%%m"
    set "USED_AI=1"
)

if "%USED_AI%"=="1" if defined COMMIT_MSG (
    echo [AI 생성 커밋 메시지]
    echo   %COMMIT_MSG%
    echo.
    goto :confirm
)

:fallback
:: fallback: 단순 메시지
set "FILE_COUNT=0"
for /f %%c in ('git status -s ^| find /c /v ""') do set "FILE_COUNT=%%c"
set "COMMIT_MSG=update: %FILE_COUNT% files changed"
echo [Fallback 커밋 메시지 ^(AI 비활성^)]
echo   %COMMIT_MSG%
echo.

:confirm
:: 사용자 확인
set /p "USER_MSG=커밋 메시지 수정 (Enter=사용, q=취소): "
if "%USER_MSG%"=="q" (
    echo 취소되었습니다.
    exit /b 0
)
if not "%USER_MSG%"=="" set "COMMIT_MSG=%USER_MSG%"

:: Stage
echo [Staging...]
git add -A

:: Commit
echo [Committing...]
git commit -m "%COMMIT_MSG%"
if errorlevel 1 (
    echo Commit 실패.
    exit /b 1
)

:: Push with retry
echo [Pushing to %BRANCH%...]
set "RETRY_DELAY=2"

for /l %%i in (1,1,4) do (
    git push -u origin %BRANCH% 2>&1
    if not errorlevel 1 (
        echo Push 성공!
        goto :cleanup
    )
    if %%i LSS 4 (
        echo Push 실패. !RETRY_DELAY!초 후 재시도...
        timeout /t !RETRY_DELAY! /nobreak >nul
        set /a "RETRY_DELAY=!RETRY_DELAY! * 2"
    )
)

echo Push 실패. 네트워크를 확인해주세요.
set "EXITCODE=1"

:cleanup
del "%DIFF_FILE%" 2>nul
del "%RESPONSE_FILE%" 2>nul
del "%REQUEST_FILE%" 2>nul
exit /b %EXITCODE%
