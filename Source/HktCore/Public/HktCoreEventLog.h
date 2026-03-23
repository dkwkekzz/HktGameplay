// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreDefs.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "HAL/CriticalSection.h"


// ============================================================================
// 로그 카테고리 GameplayTag 선언
//
// 태그 계층: HktLog.{Module}.{Level}
// MatchesTag()로 상위 태그 기준 필터링 가능.
// 예: HktLog.Core.VM.MatchesTag(HktLog.Core) == true
// ============================================================================

namespace HktLogTags
{
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Core);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Core_Entity);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Core_VM);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Core_Story);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Runtime);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Runtime_Server);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Runtime_Client);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Runtime_Intent);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Presentation);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Asset);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rule);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Story);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI);
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(VFX);
}


// ============================================================================
// HKT_EVENT_LOG 매크로
//
// ENABLE_HKT_INSIGHTS 매크로 활용 (HktCore.Build.cs 에서 비-Shipping 빌드 시 정의).
// bActive 플래그를 먼저 확인하여 패널이 닫혀 있으면 즉시 반환 (성능 최적화).
// FString::Printf 호출도 bActive 체크 이후에만 실행되므로 메모리 할당 없음.
//
// Category: HktLogTags 네임스페이스의 FGameplayTag 변수
// HKT_EVENT_LOG       — 카테고리 + 메시지만 기록
// HKT_EVENT_LOG_ENTITY — 카테고리 + 메시지 + 엔티티 ID
// HKT_EVENT_LOG_TAG   — 카테고리 + 메시지 + 엔티티 ID + GameplayTag
// ============================================================================

#if ENABLE_HKT_INSIGHTS

#define HKT_EVENT_LOG(CategoryTag, Message) \
	do { if (FHktCoreEventLog::Get().IsActive()) \
		FHktCoreEventLog::Get().Log(CategoryTag, Message); } while(0)

#define HKT_EVENT_LOG_ENTITY(CategoryTag, Message, EntityId) \
	do { if (FHktCoreEventLog::Get().IsActive()) \
		FHktCoreEventLog::Get().Log(CategoryTag, Message, EntityId); } while(0)

#define HKT_EVENT_LOG_TAG(CategoryTag, Message, EntityId, Tag) \
	do { if (FHktCoreEventLog::Get().IsActive()) \
		FHktCoreEventLog::Get().Log(CategoryTag, Message, EntityId, Tag); } while(0)

#else

#define HKT_EVENT_LOG(CategoryTag, Message)                            do {} while(0)
#define HKT_EVENT_LOG_ENTITY(CategoryTag, Message, EntityId)           do {} while(0)
#define HKT_EVENT_LOG_TAG(CategoryTag, Message, EntityId, Tag)         do {} while(0)

#endif // ENABLE_HKT_INSIGHTS


// ============================================================================
// FHktLogEntry — 개별 로그 엔트리
// ============================================================================

struct HKTCORE_API FHktLogEntry
{
	double Timestamp = 0.0;         // FPlatformTime::Seconds()
	uint64 FrameNumber = 0;         // GFrameCounter
	FGameplayTag Category;          // HktLog.Core.VM, HktLog.Runtime.Server, etc.
	FString Message;                // 자유 형식 메시지
	FHktEntityId EntityId = InvalidEntityId;  // 관련 엔티티 (-1 if none)
	FGameplayTag EventTag;          // 관련 이벤트 태그 (optional)
};


// ============================================================================
// FHktCoreEventLog
//
// 순수 C++ 싱글톤. 링 버퍼 기반 이벤트 로그 저장소.
// HktGameplay 내부에서 HKT_EVENT_LOG 매크로를 통해 이벤트를 기록하고,
// HktGameplayDeveloper의 패널이 Consume()으로 읽어 표시한다.
//
// - 패널 열림/닫힘에 따라 SetActive(true/false)로 수집 제어
// - IsActive() 체크 후 Log() 호출 (매크로가 자동 처리)
// - 링 버퍼로 메모리 상한 고정 (MaxEntries = 8192)
// - Thread-Safe: FCriticalSection 보호
// - Category는 FGameplayTag: MatchesTag()로 계층적 필터링 지원
// ============================================================================

class HKTCORE_API FHktCoreEventLog
{
public:
	static FHktCoreEventLog& Get();

	/** 로그 추가. bActive==false면 즉시 반환. */
	void Log(const FGameplayTag& Category, const FString& Message,
	         FHktEntityId EntityId = InvalidEntityId,
	         FGameplayTag EventTag = FGameplayTag());

	/** 패널에서 호출: 수집 활성화/비활성화 */
	void SetActive(bool bNewActive);

	/** 수집 활성 여부 (매크로에서 사용, lock-free) */
	bool IsActive() const { return bActive; }

	/**
	 * 패널에서 호출: InOutReadIndex 이후의 새 엔트리 반환.
	 * InOutReadIndex는 현재 WriteIndex로 갱신됨.
	 */
	TArray<FHktLogEntry> Consume(uint32& InOutReadIndex) const;

	/** 전체 로그 초기화 */
	void Clear();

	/**
	 * 현재 버퍼의 로그를 파일로 출력.
	 * 기본 경로: {ProjectDir}/Saved/Logs/HktEventLog.log
	 * @return 출력된 파일의 절대 경로. 실패 시 빈 문자열.
	 */
	FString DumpToFile(const FString& OptionalPath = TEXT("")) const;

	/** 변경 감지용 버전 카운터 */
	uint32 GetVersion() const { return Version; }

	/** 등록된 모든 카테고리 태그 반환 */
	FGameplayTagContainer GetCategories() const;

private:
	FHktCoreEventLog() = default;

	static constexpr int32 MaxEntries = 8192;

	TArray<FHktLogEntry> Entries;   // 링 버퍼 (MaxEntries로 고정 크기)
	uint32 WriteIndex = 0;          // 다음 쓰기 위치 (monotonic, % MaxEntries로 인덱싱)
	uint32 Version = 0;
	bool bActive = false;

	FGameplayTagContainer KnownCategories;  // 지금까지 기록된 카테고리 태그

	mutable FCriticalSection Lock;
};
