// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreMinimal.h"
#include "HktEvents.h"
#include "HktWorldState.h"
#include "HktSimulationDiff.h"
#include "HktRuntimeTypes.generated.h"

// =========================================================================
// [런타임 이벤트 / 네트워크 래퍼] (HKTRUNTIME 모듈)
// FHktEvent를 소유하며 암시적 변환 및 커스텀 직렬화를 제공합니다.
// =========================================================================
USTRUCT(BlueprintType)
struct HKTRUNTIME_API FHktRuntimeEvent
{
    GENERATED_BODY()

    // 1. 코어 이벤트를 내부에 값으로 소유 (동적 할당 및 포인터 연산 없음)
    FHktEvent CoreEvent;

    FHktRuntimeEvent() = default;

    // 이동 생성자: 제로 카피로 코어 이벤트 소유권 이전
    explicit FHktRuntimeEvent(FHktEvent&& InEvent)
        : CoreEvent(MoveTemp(InEvent))
    {}

    // 복사 생성자
    explicit FHktRuntimeEvent(const FHktEvent& InEvent)
        : CoreEvent(InEvent)
    {}

    // 대입 연산자: FHktEvent를 FHktRuntimeEvent에 대입
    FHktRuntimeEvent& operator=(const FHktEvent& InEvent)
    {
        CoreEvent = InEvent;
        return *this;
    }

    // 이동 대입 연산자
    FHktRuntimeEvent& operator=(FHktEvent&& InEvent)
    {
        CoreEvent = MoveTemp(InEvent);
        return *this;
    }

    // 2. 암시적 형변환 연산자
    // 이 구조체를 FHktEvent의 참조처럼 즉시 사용할 수 있게 합니다.
    FORCEINLINE operator FHktEvent&() { return CoreEvent; }
    FORCEINLINE operator const FHktEvent&() const { return CoreEvent; }

    // 편의를 위한 포인터 접근 연산자 오버로딩 (RuntimeEvent->EventId 형태로 사용 가능)
    FORCEINLINE FHktEvent* operator->() { return &CoreEvent; }
    FORCEINLINE const FHktEvent* operator->() const { return &CoreEvent; }

    // 3. 커스텀 직렬화 (엔진의 UHT 리플렉션을 거치지 않고 직접 바이트 전송)
    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
    {
        Ar << CoreEvent.EventId;
        Ar << CoreEvent.EventTag;
        Ar << CoreEvent.SourceEntity;
        Ar << CoreEvent.TargetEntity;
        Ar << CoreEvent.Location;
        Ar << CoreEvent.Param0;
        Ar << CoreEvent.Param1;

        bOutSuccess = true;
        return true;
    }

    bool operator==(const FHktRuntimeEvent& Other) const
    {
        return CoreEvent.EventId == Other.CoreEvent.EventId;
    }

    bool operator!=(const FHktRuntimeEvent& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FHktRuntimeEvent& Other) const
    {
        return CoreEvent.EventId < Other.CoreEvent.EventId;
    }

    bool IsValid() const 
    { 
        return CoreEvent.EventId != 0; 
    }
};

// 엔진에게 커스텀 직렬화 함수가 존재함을 알림
template<>
struct TStructOpsTypeTraits<FHktRuntimeEvent> : public TStructOpsTypeTraitsBase2<FHktRuntimeEvent>
{
    enum { WithNetSerializer = true };
};

//=============================================================================
// FHktRuntimeBatch - 기존 유저용 "입력(Input)" 패킷
// FHktSimulationEvent를 소유하며 암시적 변환 및 커스텀 직렬화를 제공합니다.
//=============================================================================
USTRUCT()
struct HKTRUNTIME_API FHktRuntimeBatch
{
    GENERATED_BODY()

    // 1. 코어 시뮬레이션 이벤트를 내부에 값으로 소유 (동적 할당 및 포인터 연산 없음)
    FHktSimulationEvent CoreEvent;

    FHktRuntimeBatch() = default;

    // 이동 생성자: 제로 카피로 코어 이벤트 소유권 이전
    explicit FHktRuntimeBatch(FHktSimulationEvent&& InEvent)
        : CoreEvent(MoveTemp(InEvent))
    {}

    // 복사 생성자
    explicit FHktRuntimeBatch(const FHktSimulationEvent& InEvent)
        : CoreEvent(InEvent)
    {}

    // 대입 연산자: FHktSimulationEvent를 FHktRuntimeBatch에 대입
    FHktRuntimeBatch& operator=(const FHktSimulationEvent& InEvent)
    {
        CoreEvent = InEvent;
        return *this;
    }

    // 이동 대입 연산자
    FHktRuntimeBatch& operator=(FHktSimulationEvent&& InEvent)
    {
        CoreEvent = MoveTemp(InEvent);
        return *this;
    }

    // 2. 암시적 형변환 연산자
    // 이 구조체를 FHktSimulationEvent의 참조처럼 즉시 사용할 수 있게 합니다.
    FORCEINLINE operator FHktSimulationEvent&() { return CoreEvent; }
    FORCEINLINE operator const FHktSimulationEvent&() const { return CoreEvent; }

    // 편의를 위한 포인터 접근 연산자 오버로딩 (RuntimeBatch->FrameNumber 형태로 사용 가능)
    FORCEINLINE FHktSimulationEvent* operator->() { return &CoreEvent; }
    FORCEINLINE const FHktSimulationEvent* operator->() const { return &CoreEvent; }

    // 3. 커스텀 직렬화 (엔진의 UHT 리플렉션을 거치지 않고 직접 바이트 전송)
    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
    {
        Ar << CoreEvent.FrameNumber;
        Ar << CoreEvent.RandomSeed;
        Ar << CoreEvent.DeltaSeconds;
        Ar << CoreEvent.RemovedOwnerIds;
        Ar << CoreEvent.Events; // TArray<FHktEvent> 자체 직렬화 지원
        Ar << CoreEvent.NewEntityStates;  // ← 추가

        bOutSuccess = true;
        return true;
    }

    void Reset()
    {
        CoreEvent.FrameNumber = 0;
        CoreEvent.RandomSeed = 0;
        CoreEvent.DeltaSeconds = 0.0f;
        CoreEvent.RemovedOwnerIds.Reset();
        CoreEvent.Events.Reset();
        CoreEvent.NewEntityStates.Reset();
    }
};

// 엔진에게 커스텀 직렬화 함수가 존재함을 알림
template<>
struct TStructOpsTypeTraits<FHktRuntimeBatch> : public TStructOpsTypeTraitsBase2<FHktRuntimeBatch>
{
    enum { WithNetSerializer = true };
};

//=============================================================================
// FHktRuntimeSimulationState - 신규 유저용 "결과(Result)" 패킷
// FHktWorldState를 소유하며 암시적 변환 및 커스텀 직렬화를 제공합니다.
//=============================================================================
// [중요] 그룹의 시뮬레이션 결과를 완벽하게 복원하기 위한 모든 데이터를 포함해야 함
USTRUCT()
struct HKTRUNTIME_API FHktRuntimeSimulationState
{
    GENERATED_BODY()

    // 1. 코어 월드 상태를 내부에 값으로 소유 (동적 할당 및 포인터 연산 없음)
    FHktWorldState CoreState;

    FHktRuntimeSimulationState() = default;

    // 이동 생성자: 제로 카피로 코어 상태 소유권 이전
    explicit FHktRuntimeSimulationState(FHktWorldState&& InState)
        : CoreState(MoveTemp(InState))
    {}

    // 복사 생성자
    explicit FHktRuntimeSimulationState(const FHktWorldState& InState)
        : CoreState(InState)
    {}

    // 대입 연산자: FHktWorldState를 FHktRuntimeSimulationState에 대입
    FHktRuntimeSimulationState& operator=(const FHktWorldState& InState)
    {
        CoreState = InState;
        return *this;
    }

    // 이동 대입 연산자
    FHktRuntimeSimulationState& operator=(FHktWorldState&& InState)
    {
        CoreState = MoveTemp(InState);
        return *this;
    }

    // 2. 암시적 형변환 연산자
    // 이 구조체를 FHktWorldState의 참조처럼 즉시 사용할 수 있게 합니다.
    FORCEINLINE operator FHktWorldState&() { return CoreState; }
    FORCEINLINE operator const FHktWorldState&() const { return CoreState; }

    // 편의를 위한 포인터 접근 연산자 오버로딩 (SimulationState->FrameNumber 형태로 사용 가능)
    FORCEINLINE FHktWorldState* operator->() { return &CoreState; }
    FORCEINLINE const FHktWorldState* operator->() const { return &CoreState; }

    // 3. 커스텀 직렬화 (엔진의 UHT 리플렉션을 거치지 않고 직접 바이트 전송)
    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
    {
        Ar << CoreState;

        bOutSuccess = true;
        return true;
    }

    // 편의 메서드: LastProcessedFrameNumber 접근
    int64 GetLastProcessedFrameNumber() const { return CoreState.FrameNumber; }
    void SetLastProcessedFrameNumber(int64 FrameNumber) { CoreState.FrameNumber = FrameNumber; }
};

// 엔진에게 커스텀 직렬화 함수가 존재함을 알림
template<>
struct TStructOpsTypeTraits<FHktRuntimeSimulationState> : public TStructOpsTypeTraitsBase2<FHktRuntimeSimulationState>
{
    enum { WithNetSerializer = true };
};

//=============================================================================
// FHktRuntimeDiff - 기존 유저용 "프레임 Diff" 패킷 (Legacy / Proxy 시뮬레이터)
// FHktSimulationDiff를 소유하며 암시적 변환 및 복제 지원.
//=============================================================================
USTRUCT()
struct HKTRUNTIME_API FHktRuntimeDiff
{
	GENERATED_BODY()

	FHktSimulationDiff CoreDiff;

	FHktRuntimeDiff() = default;
	explicit FHktRuntimeDiff(const FHktSimulationDiff& InDiff) : CoreDiff(InDiff) {}
	explicit FHktRuntimeDiff(FHktSimulationDiff&& InDiff) : CoreDiff(MoveTemp(InDiff)) {}

	FORCEINLINE operator FHktSimulationDiff&() { return CoreDiff; }
	FORCEINLINE operator const FHktSimulationDiff&() const { return CoreDiff; }

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << CoreDiff.FrameNumber;
		Ar << CoreDiff.SpawnedEntities;
		Ar << CoreDiff.RemovedEntities;
		Ar << CoreDiff.PropertyDeltas;
		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FHktRuntimeDiff> : public TStructOpsTypeTraitsBase2<FHktRuntimeDiff>
{
	enum { WithNetSerializer = true };
};
