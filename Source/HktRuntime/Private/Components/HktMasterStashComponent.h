#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktCoreInterfaces.h"
#include "HktCoreTypes.h"
#include "HktMasterStashComponent.generated.h"

/**
 * UHktMasterStashComponent - 서버 전용 Stash 래퍼 컴포넌트
 * 
 * HktCore의 FHktMasterStash를 래핑하여 UActorComponent로 제공
 * 
 * 역할:
 * - VMProcessor에 Stash 인터페이스 제공 (GetStashInterface)
 * - Relevancy 계산을 위한 위치 조회 (TryGetPosition)
 * - 클라이언트 전송을 위한 스냅샷 생성 (CreateEntitySnapshot)
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktMasterStashComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHktMasterStashComponent();
    
    // ========== Core Access ==========
    
    /** VMProcessor 초기화용 인터페이스 */
    IHktStashInterface* GetStashInterface() const { return Stash.Get(); }
    
    /** 내부 MasterStash 인터페이스 (필요 시 확장 기능 접근) */
    IHktMasterStashInterface* GetStash() const { return Stash.Get(); }
    
    // ========== Relevancy 지원 ==========
    
    /** 엔티티 위치 조회 (Relevancy 셀 계산용) */
    bool TryGetPosition(FHktEntityId Entity, FVector& OutPosition) const
    {
        return Stash && Stash->TryGetPosition(Entity, OutPosition);
    }
    
    // ========== 스냅샷 생성 ==========
    
    /** 단일 엔티티 스냅샷 생성 (클라이언트 전송용) */
    FHktEntitySnapshot CreateEntitySnapshot(FHktEntityId Entity) const
    {
        return Stash ? Stash->CreateEntitySnapshot(Entity) : FHktEntitySnapshot();
    }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TUniquePtr<IHktMasterStashInterface> Stash;
};
