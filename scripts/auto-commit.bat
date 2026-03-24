@echo off
chcp 65001 >nul 2>&1
setlocal EnableDelayedExpansion

:: auto-commit.bat - Git 변경점 자동 감지, 커밋 메시지 생성, commit & push
:: Usage: scripts\auto-commit.bat [branch-name]

:: repo root로 이동
for /f "delims=" %%i in ('git rev-parse --show-toplevel') do set "REPO_ROOT=%%i"
cd /d "%REPO_ROOT%"

:: 브랜치 결정
if "%~1"=="" (
    for /f "delims=" %%b in ('git branch --show-current') do set "BRANCH=%%b"
) else (
    set "BRANCH=%~1"
)

echo === HktGameplay Auto Commit ^& Push ===
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

:: 변경점 출력
echo [변경점]
git status -s
echo.

:: 파일 수 카운트
set "FILE_COUNT=0"
for /f %%c in ('git status -s ^| find /c /v ""') do set "FILE_COUNT=%%c"

:: 커밋 메시지 생성
set "COMMIT_MSG=update: %FILE_COUNT% files changed"

echo [자동 생성 커밋 메시지]
echo   %COMMIT_MSG%
echo.

:: 사용자 입력
set /p "USER_MSG=커밋 메시지 수정 (Enter=자동, q=취소): "
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
        exit /b 0
    )
    if %%i LSS 4 (
        echo Push 실패. !RETRY_DELAY!초 후 재시도...
        timeout /t !RETRY_DELAY! /nobreak >nul
        set /a "RETRY_DELAY=!RETRY_DELAY! * 2"
    )
)

echo Push 실패. 네트워크를 확인해주세요.
exit /b 1
