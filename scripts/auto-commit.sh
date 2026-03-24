#!/bin/bash
# auto-commit.sh - Git 변경점 자동 감지, 커밋 메시지 생성, commit & push
# Usage: ./scripts/auto-commit.sh [branch-name]
#   branch-name: push할 브랜치 (기본값: 현재 브랜치)

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

BRANCH="${1:-$(git branch --show-current)}"

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=== HktGameplay Auto Commit & Push ===${NC}"
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

if [ -n "$STAGED" ]; then
    echo -e "  ${GREEN}Staged:${NC}"
    echo "$STAGED" | sed 's/^/    /'
fi

if [ -n "$MODIFIED" ]; then
    echo -e "  ${YELLOW}Modified:${NC}"
    echo "$MODIFIED" | sed 's/^/    /'
fi

if [ -n "$UNTRACKED" ]; then
    echo -e "  ${RED}Untracked:${NC}"
    echo "$UNTRACKED" | sed 's/^/    /'
fi

echo ""

# 3. 커밋 메시지 자동 생성
generate_commit_message() {
    local all_files=""
    [ -n "$STAGED" ] && all_files="$STAGED"
    [ -n "$MODIFIED" ] && all_files="${all_files:+$all_files$'\n'}$MODIFIED"
    [ -n "$UNTRACKED" ] && all_files="${all_files:+$all_files$'\n'}$UNTRACKED"

    # 파일 확장자/경로 기반 변경 유형 분석
    local has_cpp=false has_h=false has_cs=false has_config=false has_script=false
    local dirs=()

    while IFS= read -r file; do
        case "$file" in
            *.cpp) has_cpp=true ;;
            *.h)   has_h=true ;;
            *.cs)  has_cs=true ;;
            *.ini|*.json|*.yaml|*.yml) has_config=true ;;
            *.sh|*.bat|*.py) has_script=true ;;
        esac
        # 최상위 디렉토리 수집
        local dir
        dir=$(echo "$file" | cut -d'/' -f1-2)
        dirs+=("$dir")
    done <<< "$all_files"

    # 고유 디렉토리
    local unique_dirs
    unique_dirs=$(printf '%s\n' "${dirs[@]}" | sort -u | head -3)
    local dir_summary
    dir_summary=$(echo "$unique_dirs" | tr '\n' ', ' | sed 's/,$//')

    # 파일 수
    local file_count
    file_count=$(echo "$all_files" | sort -u | wc -l | tr -d ' ')

    # prefix 결정
    local prefix="update"
    local added_count=0 modified_count=0
    [ -n "$UNTRACKED" ] && added_count=$(echo "$UNTRACKED" | wc -l | tr -d ' ')
    [ -n "$MODIFIED" ] && modified_count=$(echo "$MODIFIED" | wc -l | tr -d ' ')

    if [ "$added_count" -gt 0 ] && [ "$modified_count" -eq 0 ]; then
        prefix="add"
    elif [ "$modified_count" -gt 0 ] && [ "$added_count" -eq 0 ]; then
        # diff에서 삭제 비율 확인
        local insertions deletions
        insertions=$(git diff --shortstat 2>/dev/null | grep -oP '\d+(?= insertion)' || echo "0")
        deletions=$(git diff --shortstat 2>/dev/null | grep -oP '\d+(?= deletion)' || echo "0")
        if [ "${deletions:-0}" -gt "${insertions:-0}" ]; then
            prefix="fix"
        fi
    fi

    echo "${prefix}: ${dir_summary} (${file_count} files)"
}

AUTO_MSG=$(generate_commit_message)

echo -e "${GREEN}[자동 생성 커밋 메시지]${NC}"
echo -e "  ${YELLOW}${AUTO_MSG}${NC}"
echo ""

# 4. 사용자 확인 (interactive mode)
if [ -t 0 ]; then
    echo -e "커밋 메시지를 수정하려면 입력하세요 (Enter로 자동 메시지 사용, 'q'로 취소):"
    read -r USER_MSG
    if [ "$USER_MSG" = "q" ]; then
        echo -e "${RED}취소되었습니다.${NC}"
        exit 0
    fi
    if [ -n "$USER_MSG" ]; then
        AUTO_MSG="$USER_MSG"
    fi
fi

# 5. Stage all changes
echo -e "${GREEN}[Staging changes...]${NC}"
git add -A

# 6. Commit
echo -e "${GREEN}[Committing...]${NC}"
git commit -m "$AUTO_MSG"

# 7. Push with retry (exponential backoff)
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
