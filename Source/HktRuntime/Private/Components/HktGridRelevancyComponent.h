#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktRelevancyProvider.h"
#include "HktRuntimeTypes.h"
#include "HktGridRelevancyComponent.generated.h"

class AHktPlayerController;
class UHktMasterStashComponent;

/**
 * 플레이어별 그리드 캐시 정보
 */
USTRUCT()
struct FHktPlayerGridCache
{
    GENERATED_BODY()

    FIntPoint CurrentCell = FIntPoint(INT_MAX, INT_MAX);
    FVector LastLocation = FVector::ZeroVector;

    // 구독 중인 셀 (O(1) 조회용 Set)
    TSet<FIntPoint> SubscribedCellSet;

    // 현재 보이는 엔티티 (Relevancy 범위 내)
    TSet<FHktEntityId> VisibleEntities;

    // 이번 프레임에 새로 보이는 엔티티 (스냅샷 필요)
    TArray<FHktEntityId> EnteredEntities;

    // 이번 프레임에 벗어난 엔티티 (제거 알림)
    TArray<FHktEntityId> ExitedEntities;

    void BeginFrame()
    {
        EnteredEntities.Reset();
        ExitedEntities.Reset();
    }
};

/**
 * UHktGridRelevancyComponent
 *
 * 이벤트 기반 그리드 Relevancy 정책
 * - 엔티티 셀 변경 이벤트를 받아 클라이언트별 Relevancy 업데이트
 * - 매 틱 전체 순회 없이 O(변경 수) 처리
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktGridRelevancyComponent : public UActorComponent, public IHktRelevancyProvider
{
    GENERATED_BODY()

public:
    UHktGridRelevancyComponent();

    // === IHktRelevancyProvider 구현 ===

    virtual void RegisterClient(AHktPlayerController* Client) override;
    virtual void UnregisterClient(AHktPlayerController* Client) override;
    virtual const TArray<AHktPlayerController*>& GetAllClients() const override { return ValidClients; }

    virtual void GetRelevantClientsAtLocation(
        const FVector& Location,
        TArray<AHktPlayerController*>& OutRelevantClients
    ) override;

    virtual void GetAllRelevantClients(TArray<AHktPlayerController*>& OutRelevantClients) override
    {
        OutRelevantClients = ValidClients;
    }

    virtual void UpdateRelevancy() override;

    // === 셀 조회 ===

    // 위치 → 셀 변환
    FIntPoint LocationToCell(const FVector& Location) const;

    // 클라이언트가 해당 셀에 관심 있는지 O(1) 체크
    bool IsClientInterestedInCell(AHktPlayerController* Client, FIntPoint Cell) const;

    // 글로벌 이벤트용
    bool IsClientInterestedInGlobal(AHktPlayerController* Client) const { return true; }

    // === 엔티티 Relevancy 조회 ===

    /** 클라이언트의 Relevancy 범위 내 모든 엔티티 조회 */
    TArray<FHktEntityId> GetEntitiesInRelevancy(AHktPlayerController* Client) const;

    /** 클라이언트에게 이번 프레임에 새로 보이는 엔티티 조회 (스냅샷 전송용) */
    TArray<FHktEntityId> GetNewlyVisibleEntities(AHktPlayerController* Client) const;

    /** 클라이언트의 Relevancy를 벗어난 엔티티 조회 (제거 전송용) */
    TArray<FHktEntityId> GetRemovedEntities(AHktPlayerController* Client) const;

    /** MasterStash 설정 */
    void SetMasterStash(UHktMasterStashComponent* InMasterStash);

    // === 설정 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Grid")
    float CellSize = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Grid")
    int32 InterestRadius = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Grid")
    float MovementThreshold = 100.0f;

protected:
    FVector GetPlayerLocation(AHktPlayerController* PC) const;

    /** 플레이어 구독 셀 업데이트 (셀 변경 시) */
    void UpdatePlayerSubscription(AHktPlayerController* PC, FHktPlayerGridCache& Cache);

    /** 셀 변경 이벤트 처리 */
    void ProcessCellChangeEvents();

    /** 새 클라이언트의 초기 Relevancy 구축 */
    void InitializeClientRelevancy(AHktPlayerController* PC, FHktPlayerGridCache& Cache);

private:
    TArray<TWeakObjectPtr<AHktPlayerController>> RegisteredClients;
    TArray<AHktPlayerController*> ValidClients;
    TMap<AHktPlayerController*, FHktPlayerGridCache> PlayerCaches;

    // 새로 등록된 클라이언트 (초기 스냅샷 필요)
    TArray<AHktPlayerController*> NewClients;

    UPROPERTY()
    TWeakObjectPtr<UHktMasterStashComponent> MasterStash;
};
