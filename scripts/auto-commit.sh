#!/bin/bash
# auto-commit.sh - AI agent가 git diff를 분석하여 커밋 메시지를 생성, commit & push
# Usage: ./scripts/auto-commit.sh [branch-name]
#
# 환경변수:
#   ANTHROPIC_API_KEY  - Claude API 키 (필수)
#   COMMIT_MODEL       - 사용할 모델 (기본값: claude-haiku-4-5-20251001)
#   NO_AI              - "1"로 설정하면 AI 없이 fallback 메시지 사용

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

BRANCH="${1:-$(git branch --show-current)}"
MODEL="${COMMIT_MODEL:-claude-haiku-4-5-20251001}"

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${GREEN}=== HktGameplay Auto Commit & Push (AI) ===${NC}"
echo -e "Branch: ${YELLOW}${BRANCH}${NC}"
echo ""

# 1. 변경점 확인
STAGED=$(git diff --cached --name-only)
MODIFIED=$(git diff --name-only)
UNTRACKED=$(git ls-files --others --exclude-standard)

if [ -z "$STAGED" ] && [ -z "$MODIFIED" ] && [ -z "$UNTRACKED" ]; then
    echo -e "${YELLOW}변경점이 없습니다. 종료합니다.${NC}"
    exit 0
fi

# 2. 변경점 요약 출력
echo -e "${GREEN}[변경점 요약]${NC}"
git status -s
echo ""

# 3. diff 수집 (AI에게 전달할 컨텍스트)
collect_diff_context() {
    local context=""

    # staged + unstaged diff (최대 4000자로 제한)
    local diff_output
    diff_output=$(git diff --cached --stat 2>/dev/null; git diff --stat 2>/dev/null)
    local diff_detail
    diff_detail=$(git diff --cached -U3 2>/dev/null; git diff -U3 2>/dev/null)

    # untracked 파일 내용 (신규 파일은 첫 20줄만)
    local untracked_preview=""
    if [ -n "$UNTRACKED" ]; then
        while IFS= read -r f; do
            if [ -f "$f" ]; then
                untracked_preview="${untracked_preview}--- NEW: ${f} ---
$(head -20 "$f" 2>/dev/null)
"
            fi
        done <<< "$UNTRACKED"
    fi

    # 최근 커밋 5개 (스타일 참고용)
    local recent_commits
    recent_commits=$(git log --oneline -5 2>/dev/null || echo "")

    # 전체 컨텍스트를 4000자로 제한
    context="[File Stats]
${diff_output}

[Diff Detail]
${diff_detail}

[New Files Preview]
${untracked_preview}

[Recent Commits for Style Reference]
${recent_commits}"

    echo "${context:0:4000}"
}

# 4. AI로 커밋 메시지 생성
generate_ai_commit_message() {
    local diff_context="$1"

    # API 키 체크
    if [ -z "${ANTHROPIC_API_KEY:-}" ] || [ "${NO_AI:-0}" = "1" ]; then
        return 1
    fi

    # jq 존재 확인
    if ! command -v jq &>/dev/null; then
        echo -e "${YELLOW}jq가 설치되어 있지 않습니다. AI 메시지 생성을 건너뜁니다.${NC}" >&2
        return 1
    fi

    echo -e "${CYAN}[AI가 커밋 메시지를 분석 중...]${NC}" >&2

    local escaped_context
    escaped_context=$(echo "$diff_context" | jq -Rs .)

    local request_body
    request_body=$(cat <<REQEOF
{
  "model": "${MODEL}",
  "max_tokens": 256,
  "messages": [
    {
      "role": "user",
      "content": "You are a commit message generator for an Unreal Engine 5 MMORPG project (HktGameplay). Analyze the git diff below and write a concise, conventional commit message.\n\nRules:\n- Format: <type>: <description>\n- Types: feat, fix, refactor, docs, chore, test, style\n- Description in English, max 72 chars\n- If there's a body needed, add a blank line then 1-2 bullet points\n- Focus on WHAT changed and WHY, not HOW\n- Match the style of recent commits shown below\n\nGit diff context:\n${diff_context}\n\nRespond with ONLY the commit message, nothing else."
    }
  ]
}
REQEOF
)

    local response
    response=$(curl -s --max-time 15 \
        -H "Content-Type: application/json" \
        -H "x-api-key: ${ANTHROPIC_API_KEY}" \
        -H "anthropic-version: 2023-06-01" \
        -d "$request_body" \
        "https://api.anthropic.com/v1/messages" 2>/dev/null)

    # 응답에서 메시지 추출
    local msg
    msg=$(echo "$response" | jq -r '.content[0].text // empty' 2>/dev/null)

    if [ -n "$msg" ]; then
        echo "$msg"
        return 0
    fi

    # 에러 출력
    local err
    err=$(echo "$response" | jq -r '.error.message // empty' 2>/dev/null)
    if [ -n "$err" ]; then
        echo -e "${RED}API 에러: ${err}${NC}" >&2
    fi
    return 1
}

# 5. fallback: 단순 규칙 기반 메시지
generate_fallback_message() {
    local file_count
    file_count=$(git status -s | wc -l | tr -d ' ')
    local dirs
    dirs=$(git status -s | awk '{print $2}' | cut -d'/' -f1-2 | sort -u | head -3 | tr '\n' ', ' | sed 's/,$//')
    echo "update: ${dirs} (${file_count} files)"
}

# 6. 메시지 생성 실행
DIFF_CONTEXT=$(collect_diff_context)

if AI_MSG=$(generate_ai_commit_message "$DIFF_CONTEXT"); then
    AUTO_MSG="$AI_MSG"
    echo -e "${GREEN}[AI 생성 커밋 메시지]${NC}"
else
    AUTO_MSG=$(generate_fallback_message)
    echo -e "${YELLOW}[Fallback 커밋 메시지 (AI 비활성)]${NC}"
fi

echo -e "  ${CYAN}${AUTO_MSG}${NC}"
echo ""

# 7. 사용자 확인 (interactive mode)
if [ -t 0 ]; then
    echo -e "커밋 메시지를 수정하려면 입력하세요 (Enter=AI 메시지 사용, 'q'=취소):"
    read -r USER_MSG
    if [ "$USER_MSG" = "q" ]; then
        echo -e "${RED}취소되었습니다.${NC}"
        exit 0
    fi
    if [ -n "$USER_MSG" ]; then
        AUTO_MSG="$USER_MSG"
    fi
fi

# 8. Stage all changes
echo -e "${GREEN}[Staging changes...]${NC}"
git add -A

# 9. Commit
echo -e "${GREEN}[Committing...]${NC}"
git commit -m "$AUTO_MSG"

# 10. Push with retry (exponential backoff)
echo -e "${GREEN}[Pushing to ${BRANCH}...]${NC}"
MAX_RETRIES=4
RETRY_DELAY=2

for i in $(seq 1 $MAX_RETRIES); do
    if git push -u origin "$BRANCH" 2>&1; then
        echo -e "${GREEN}Push 성공!${NC}"
        exit 0
    fi
    if [ "$i" -lt "$MAX_RETRIES" ]; then
        echo -e "${YELLOW}Push 실패. ${RETRY_DELAY}초 후 재시도... (${i}/${MAX_RETRIES})${NC}"
        sleep $RETRY_DELAY
        RETRY_DELAY=$((RETRY_DELAY * 2))
    fi
done

echo -e "${RED}Push 실패. 네트워크를 확인해주세요.${NC}"
exit 1
