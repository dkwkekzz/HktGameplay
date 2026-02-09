// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktCoreTypes.h"
#include "HktCoreInterfaces.h"
#include "HktVMProcessorComponent.generated.h"

/**
 * UHktVMProcessorComponent - FHktVMProcessor를 랩핑하는 컴포넌트
 * 
 * 서버(GameMode)와 클라이언트(PlayerController) 모두에서 사용 가능
 * - 서버: MasterStash와 연결하여 권위 있는 시뮬레이션 실행
 * - 클라: VisibleStash와 연결하여 로컬 시뮬레이션 실행
 * 
 * 사용법:
 *   1. Initialize() 호출 (MasterStashComponent 또는 VisibleStashComponent 전달)
 *   2. NotifyIntentEvent()로 이벤트 알림
 *   3. ProcessFrame()으로 프레임 처리
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktVMProcessorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHktVMProcessorComponent();
    ~UHktVMProcessorComponent();

    // ========== Initialization ==========
    
    /** MasterStashComponent로 초기화 */
    void Initialize(IHktStashInterface* InStash);
    
    /** 초기화 여부 */
    bool IsInitialized() const { return VMProcessor.IsValid(); }

    // ========== Event Notifications ==========
    
    /** 단일 Intent 이벤트 알림 */
    void NotifyIntentEvent(int32 InFrameNumber, const FHktIntentEvent& Event);
    
    /** 여러 Intent 이벤트 일괄 알림 */
    void NotifyIntentEvents(int32 InFrameNumber, const TArray<FHktIntentEvent>& Events);

    // ========== Notifications ==========
    
    /** 충돌 알림 */
    void NotifyCollision(FHktEntityId WatchedEntity, FHktEntityId HitEntity);
    
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** 내부 VMProcessor 인스턴스 (인터페이스로 접근) */
    TUniquePtr<IHktVMProcessorInterface> VMProcessor;

    int32 SyncFrameNumber = 0;
};
