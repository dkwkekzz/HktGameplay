// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktServerRule.h"
#include "HktPersistentFrameComponent.generated.h"

class IHktPersistentFrameProvider;

/**
 * UHktPersistentFrameComponent - IHktPersistentFrame 구현
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - Actor(GameMode)는 이 컴포넌트를 Rule에 IHktPersistentFrame으로 전달
 *
 * 역할:
 *   - Hi-Lo 배치 할당으로 영구 프레임 번호 제공
 *   - Provider 패턴으로 저장소 교체 가능 (파일 → Redis)
 */
UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktPersistentFrameComponent : public UActorComponent, public IHktPersistentFrame
{
    GENERATED_BODY()

public:
    UHktPersistentFrameComponent();

    // === IHktPersistentFrame 구현 ===

    virtual bool IsInitialized() const override;
    virtual uint64 GetFrameNumber() const override;
    virtual void AdvanceFrame() override;

    /** 현재 프레임의 DeltaSeconds (Tick에서 갱신) */
    float GetDeltaSeconds() const { return CachedDeltaSeconds; }

    /** Tick에서 DeltaSeconds를 업데이트 (GameMode에서 호출) */
    void SetDeltaSeconds(float InDelta) { CachedDeltaSeconds = InDelta; }

protected:
    virtual void BeginPlay() override;

private:
    void ReserveNextBatch();

    UPROPERTY(EditDefaultsOnly, Category = "Hkt|PersistentTick", meta = (ClampMin = "1000", ClampMax = "1000000"))
    int64 BatchSize = 36000;

    int64 ReservedMaxFrame = 0;
    int64 CurrentFrame = 0;
    float CachedDeltaSeconds = 0.0f;
    bool bIsReservePending = false;
    bool bIsInitialized = false;

    TUniquePtr<IHktPersistentFrameProvider> Provider;
};
