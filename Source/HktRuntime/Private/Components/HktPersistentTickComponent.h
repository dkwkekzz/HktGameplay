// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktPersistentTickProvider.h"
#include "HktPersistentTickComponent.generated.h"

/**
 * UHktPersistentTickComponent - 영구 논리 틱(Logical Tick) 제공
 *
 * Hi-Lo 배치 할당으로 DB/파일에 저장 가능한 연속 프레임 번호를 제공합니다.
 * GameMode에 부착하여 사용. 서버 전용 (GameMode는 서버에만 존재).
 *
 * 사용법:
 *   1. GameMode 생성자에서 CreateDefaultSubobject
 *   2. Tick 시작 시 AdvanceFrame() 호출하여 프레임 진행
 *   3. GetCurrentPersistentFrame()으로 현재 프레임 조회
 *
 * 범위 초과 시: 로그 후 대기 (AdvanceFrame이 진행하지 않음, 다음 틱에 재시도).
 */
UCLASS(ClassGroup = (HktSimulation), meta = (BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktPersistentTickComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHktPersistentTickComponent();

    /** 현재 유효한 영구 프레임 번호 */
    UFUNCTION(BlueprintPure, Category = "Hkt|PersistentTick")
    int64 GetCurrentPersistentFrame() const { return CurrentFrame; }

    /**
     * 프레임을 1 진행하고 새 값을 반환.
     * 초기화 전이거나 범위 초과 시 -1 반환 (진행하지 않음).
     */
    int64 AdvanceFrame();

    /** 초기화 완료 여부 (DB/파일에서 첫 배치 수신 후 true) */
    bool IsInitialized() const { return bIsInitialized; }

protected:
    virtual void BeginPlay() override;

private:
    void ReserveNextBatch();

    UPROPERTY(EditDefaultsOnly, Category = "Hkt|PersistentTick", meta = (ClampMin = "1000", ClampMax = "1000000"))
    int64 BatchSize = 36000;

    int64 ReservedMaxFrame = 0;
    int64 CurrentFrame = 0;
    bool bIsReservePending = false;
    bool bIsInitialized = false;

    TUniquePtr<IHktPersistentTickProvider> Provider;
};
