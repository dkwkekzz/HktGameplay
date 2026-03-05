// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreEvents.h"
#include "HktWorldState.h"

// ============================================================================
// FHktVMDebugInfo — VM 실행 상태 디버그 정보 (Public, 순수 데이터)
// ============================================================================

struct HKTCORE_API FHktVMDebugInfo
{
    int32 PC = 0;
    int32 Status = 0;       // EVMStatus as int32
    FString ProgramTag;     // FGameplayTag::ToString()
    int64 PlayerUid = 0;
    int32 CreationFrame = 0;
    int32 SelfEntity = -1;
    int32 TargetEntity = -1;
    int32 SourceEventId = 0;
};

// ============================================================================
// IHktDeterminismSimulator — 클라/서버 공통 코어 시뮬레이터 (내부 시뮬레이션만)
// ============================================================================

class HKTCORE_API IHktDeterminismSimulator
{
public:
    virtual ~IHktDeterminismSimulator() = default;

    virtual FHktSimulationDiff AdvanceFrame(const FHktSimulationEvent& InEvent) = 0;
    virtual const FHktWorldState& GetWorldState() const = 0;
    virtual FHktPlayerState ExportPlayerState(int64 OwnerUid) const = 0;
    virtual void RestoreWorldState(const FHktWorldState& InState) = 0;

    /** Diff 역적용 — 프레임 변경 되돌리기 (클라이언트 예측 롤백용) */
    virtual void UndoDiff(const FHktSimulationDiff& Diff) = 0;

    /** 활성 VM 개수 (디버그/인사이트용) */
    virtual int32 GetActiveVMCount() const { return 0; }

    /** 활성 VM 순회 — FHktVMDebugInfo로 외부에 노출 */
    virtual void ForEachActiveVM(TFunctionRef<void(const FHktVMDebugInfo&)> Callback) const {}

    /** Insights 소스 이름 (Server[0], Client 등) — AdvanceFrame push에 사용 */
    FString InsightsSourceName;

    /** 소스 이름 설정 (HktRuntime에서 호출) */
    virtual void SetInsightsSourceName(const FString& Name) { InsightsSourceName = Name; }
};

// ============================================================================
// Factory
// ============================================================================

/** 클라이언트/서버 공통: 결정론 시뮬레이터 (서버는 반환값을 IHktAuthoritySimulator*로 캐스트하여 사용) */
HKTCORE_API TUniquePtr<IHktDeterminismSimulator> CreateDeterminismSimulator();
