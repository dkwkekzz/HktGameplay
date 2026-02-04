#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktCoreInterfaces.h"
#include "HktCoreTypes.h"
#include "HktVisibleStashComponent.generated.h"

/**
 * UHktVisibleStashComponent - 클라이언트 전용 Stash 래퍼 컴포넌트
 * 
 * HktCore의 FHktVisibleStash를 래핑하여 UActorComponent로 제공
 * 
 * 역할:
 * - VMProcessor에 Stash 인터페이스 제공 (GetStashInterface)
 * - 서버에서 받은 스냅샷 적용 (ApplyEntitySnapshot)
 * - 제거된 엔티티 처리 (FreeEntity)
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktVisibleStashComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHktVisibleStashComponent();
    
    // ========== Core Access ==========
    
    /** VMProcessor 초기화용 인터페이스 */
    IHktStashInterface* GetStashInterface() const { return Stash.Get(); }
    
    /** 내부 VisibleStash 인터페이스 (필요 시 확장 기능 접근) */
    IHktVisibleStashInterface* GetStash() const { return Stash.Get(); }
    
    // ========== 스냅샷 적용 ==========
    
    /** 서버에서 받은 엔티티 스냅샷 적용 */
    void ApplyEntitySnapshot(const FHktEntitySnapshot& Snapshot)
    {
        if (Stash) Stash->ApplyEntitySnapshot(Snapshot);
    }
    
    // ========== 엔티티 제거 ==========
    
    /** 엔티티 제거 (Relevancy 이탈 시) */
    void FreeEntity(FHktEntityId Entity)
    {
        if (Stash) Stash->FreeEntity(Entity);
    }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TUniquePtr<IHktVisibleStashInterface> Stash;
};
