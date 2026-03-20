// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreDefs.h"
#include "GameplayTagContainer.h"
#include "HAL/CriticalSection.h"


// ============================================================================
// FHktLogEntry — 개별 로그 엔트리
// ============================================================================

struct HKTCORE_API FHktLogEntry
{
	double Timestamp = 0.0;         // FPlatformTime::Seconds()
	uint64 FrameNumber = 0;         // GFrameCounter
	FString Category;               // "Core.Simulation", "Runtime.Server", etc.
	FString Message;                // 자유 형식 메시지
	FHktEntityId EntityId = InvalidEntityId;  // 관련 엔티티 (-1 if none)
	FGameplayTag EventTag;          // 관련 이벤트 태그 (optional)
};


// ============================================================================
// FHktCoreEventLog
//
// 순수 C++ 싱글톤. 링 버퍼 기반 이벤트 로그 저장소.
// HktGameplayDeveloper의 패널이 Consume()으로 읽어 표시한다.
//
// - 패널 열림/닫힘에 따라 SetActive(true/false)로 수집 제어
// - IsActive() 체크 후 Log() 호출 (매크로가 자동 처리)
// - 링 버퍼로 메모리 상한 고정 (MaxEntries = 8192)
// - Thread-Safe: FCriticalSection 보호
// ============================================================================

class HKTCORE_API FHktCoreEventLog
{
public:
	static FHktCoreEventLog& Get();

	/** 로그 추가. bActive==false면 즉시 반환. */
	void Log(const TCHAR* Category, const FString& Message,
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

	/** 변경 감지용 버전 카운터 */
	uint32 GetVersion() const { return Version; }

	/** 등록된 모든 카테고리 목록 반환 */
	TArray<FString> GetCategories() const;

private:
	FHktCoreEventLog() = default;

	static constexpr int32 MaxEntries = 8192;

	TArray<FHktLogEntry> Entries;   // 링 버퍼 (MaxEntries로 고정 크기)
	uint32 WriteIndex = 0;          // 다음 쓰기 위치 (monotonic, % MaxEntries로 인덱싱)
	uint32 Version = 0;
	bool bActive = false;

	TSet<FString> KnownCategories;  // 지금까지 기록된 카테고리 목록

	mutable FCriticalSection Lock;
};
