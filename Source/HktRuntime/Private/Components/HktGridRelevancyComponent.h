// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktServerRule.h"
#include "HktRuntimeTypes.h"
#include "HktGridRelevancyComponent.generated.h"

class AHktInGamePlayerController;
class UHktMasterStashComponent;

// ============================================================================
// FHktWorldPlayerAdapter - IHktWorldPlayer의 컴포넌트 내부 구현
//
// AHktInGamePlayerController를 래핑하여 IHktWorldPlayer 인터페이스 제공.
// Actor가 인터페이스를 직접 구현하지 않도록 어댑터 패턴 사용.
// ============================================================================
class FHktWorldPlayerAdapter : public IHktWorldPlayer
{
public:
    FHktWorldPlayerAdapter(AHktInGamePlayerController* InPC, int64 InPlayerUid);

    virtual int64 GetPlayerUid() const override { return PlayerUid; }
    virtual void SendFrameBatch(const FHktFrameBatch& Batch) override;
    virtual void SendInitialSimulationState(const FHktGroupSimulationState& InitialState) override;

    AHktInGamePlayerController* GetPlayerController() const { return PC.Get(); }
    bool IsValid() const { return PC.IsValid(); }

private:
    TWeakObjectPtr<AHktInGamePlayerController> PC;
    int64 PlayerUid = 0;
};

// ============================================================================
// FHktRelevancyGroupImpl - IHktRelevancyGroup의 컴포넌트 내부 구현
// ============================================================================
class FHktRelevancyGroupImpl : public IHktRelevancyGroup
{
public:
    virtual IHktSimulator& GetSimulator() override;
    virtual const TArray<int64>& GetPlayerUids() const override { return PlayerUids; }
    virtual const TArray<IHktWorldPlayer*>& GetCachedWorldPlayers() const override { return CachedPlayers; }

    void SetSimulator(IHktSimulator* InSimulator) { Simulator = InSimulator; }
    void AddPlayer(int64 Uid, IHktWorldPlayer* Player);
    void RemovePlayer(int64 Uid);
    void ClearCaches();

private:
    IHktSimulator* Simulator = nullptr;
    TArray<int64> PlayerUids;
    TArray<IHktWorldPlayer*> CachedPlayers;
};

/**
 * UHktGridRelevancyComponent - IHktRelevancyGraph 구현
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - Actor(GameMode)는 이 컴포넌트를 Rule에 IHktRelevancyGraph로 전달
 *   - IHktWorldPlayer 어댑터도 이 컴포넌트가 소유/관리
 *
 * 역할:
 *   - IHktRelevancyGraph: 그룹 관리, 플레이어 등록/해제
 *   - IHktWorldPlayer 래퍼 생명주기 관리
 *   - 셀 기반 공간 분할 Relevancy
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktGridRelevancyComponent : public UActorComponent, public IHktRelevancyGraph
{
    GENERATED_BODY()

public:
    UHktGridRelevancyComponent();

    // === IHktRelevancyGraph 구현 ===

    virtual void RegisterPlayer(IHktWorldPlayer* Player, int32 GroupIndex) override;
    virtual void UnregisterPlayer(int64 PlayerUid) override;
    virtual void UpdateRelevancy() override;
    virtual IHktWorldPlayer* GetWorldPlayer(int64 PlayerUid) const override;
    virtual int32 GetGroupIndexByLocation(const FVector& Location) const override;
    virtual int32 NumRelevancyGroup() const override;
    virtual IHktRelevancyGroup& GetRelevancyGroup(int32 Index) override;
    virtual const IHktRelevancyGroup& GetRelevancyGroup(int32 Index) const override;

    // === WorldPlayer 어댑터 관리 (GameMode에서 호출) ===

    /** PostLogin 시: PlayerController를 등록하고 WorldPlayer 어댑터를 생성/반환 */
    IHktWorldPlayer* RegisterAndWrapClient(AHktInGamePlayerController* Client);

    /** Logout 시: 클라이언트 등록 해제 */
    void UnregisterClient(AHktInGamePlayerController* Client);

    /** PlayerController로 WorldPlayer 어댑터 조회 */
    IHktWorldPlayer* FindWorldPlayer(AHktInGamePlayerController* Client) const;

    /** PlayerUid로 WorldPlayer 어댑터 조회 */
    IHktWorldPlayer* FindWorldPlayerByUid(int64 PlayerUid) const;

    /** Record 기반으로 WorldPlayer 생성 (OnTick_ProcessPendingConnections의 PlayerFactory용) */
    IHktWorldPlayer* CreateWorldPlayerFromRecord(const FHktPlayerRecord& Record);

    // === MasterStash 설정 ===
    void SetMasterStash(UHktMasterStashComponent* InMasterStash);

    // === 셀 유틸 ===
    FIntPoint LocationToCell(const FVector& Location) const;

    // === 설정 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Grid")
    float CellSize = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Grid")
    int32 InterestRadius = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Grid")
    int32 NumGroups = 1;

protected:
    virtual void BeginPlay() override;

private:
    // WorldPlayer 어댑터 (컴포넌트가 소유)
    TMap<int64, TUniquePtr<FHktWorldPlayerAdapter>> WorldPlayers;

    // PlayerController → PlayerUid 매핑
    TMap<TWeakObjectPtr<AHktInGamePlayerController>, int64> PCToUidMap;

    // RelevancyGroup 구현체
    TArray<FHktRelevancyGroupImpl> Groups;

    UPROPERTY()
    TWeakObjectPtr<UHktMasterStashComponent> MasterStash;
};
