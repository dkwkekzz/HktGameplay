#include "HktGridRelevancyComponent.h"
#include "HktPlayerController.h"
#include "HktMasterStashComponent.h"
#include "GameFramework/Pawn.h"

UHktGridRelevancyComponent::UHktGridRelevancyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// === 클라이언트 관리 ===

void UHktGridRelevancyComponent::RegisterClient(AHktPlayerController* Client)
{
    if (!Client)
    {
        return;
    }

    for (const auto& WeakPC : RegisteredClients)
    {
        if (WeakPC.Get() == Client)
        {
            return;
        }
    }

    RegisteredClients.Add(Client);

    FHktPlayerGridCache& Cache = PlayerCaches.Add(Client);

    // 새 클라이언트로 마킹 (다음 UpdateRelevancy에서 초기화)
    NewClients.Add(Client);

    UE_LOG(LogTemp, Log, TEXT("GridRelevancy: Registered client %s"), *Client->GetName());
}

void UHktGridRelevancyComponent::UnregisterClient(AHktPlayerController* Client)
{
    if (!Client)
    {
        return;
    }

    PlayerCaches.Remove(Client);
    NewClients.Remove(Client);
    RegisteredClients.RemoveAll([Client](const TWeakObjectPtr<AHktPlayerController>& WeakPC)
    {
        return !WeakPC.IsValid() || WeakPC.Get() == Client;
    });

    UE_LOG(LogTemp, Log, TEXT("GridRelevancy: Unregistered client %s"), *Client->GetName());
}

void UHktGridRelevancyComponent::SetMasterStash(UHktMasterStashComponent* InMasterStash)
{
    MasterStash = InMasterStash;

    // MasterStash에 CellSize 동기화
    if (MasterStash.IsValid())
    {
        if (IHktMasterStashInterface* Stash = MasterStash->GetStash())
        {
            Stash->SetCellSize(CellSize);
        }
    }
}

// === 업데이트 ===

void UHktGridRelevancyComponent::UpdateRelevancy()
{
    // 1. 유효한 클라이언트 목록 갱신
    ValidClients.Reset();

    for (int32 i = RegisteredClients.Num() - 1; i >= 0; --i)
    {
        AHktPlayerController* PC = RegisteredClients[i].Get();
        if (!PC)
        {
            RegisteredClients.RemoveAt(i);
            continue;
        }
        ValidClients.Add(PC);
    }

    // 2. 각 클라이언트 프레임 시작 (Enter/Exit 버퍼 초기화)
    for (AHktPlayerController* PC : ValidClients)
    {
        if (FHktPlayerGridCache* Cache = PlayerCaches.Find(PC))
        {
            Cache->BeginFrame();
        }
    }

    // 3. 각 플레이어 위치 변경 → 구독 셀 업데이트
    for (AHktPlayerController* PC : ValidClients)
    {
        FHktPlayerGridCache* Cache = PlayerCaches.Find(PC);
        if (!Cache)
        {
            continue;
        }

        FVector CurrentLocation = GetPlayerLocation(PC);
        float DistSq = FVector::DistSquared(CurrentLocation, Cache->LastLocation);

        if (DistSq > FMath::Square(MovementThreshold))
        {
            FIntPoint NewCell = LocationToCell(CurrentLocation);
            if (NewCell != Cache->CurrentCell)
            {
                // 구독 셀 변경 → 기존 Relevancy와 차집합 계산 필요
                TSet<FIntPoint> OldCells = Cache->SubscribedCellSet;

                UpdatePlayerSubscription(PC, *Cache);

                // 새로 구독한 셀의 엔티티 추가
                if (MasterStash.IsValid())
                {
                    IHktMasterStashInterface* Stash = MasterStash->GetStash();
                    if (Stash)
                    {
                        // 새로 구독한 셀 (NewCells - OldCells)
                        for (const FIntPoint& Cell : Cache->SubscribedCellSet)
                        {
                            if (!OldCells.Contains(Cell))
                            {
                                if (const TSet<FHktEntityId>* CellEntities = Stash->GetEntitiesInCell(Cell))
                                {
                                    for (FHktEntityId Entity : *CellEntities)
                                    {
                                        if (!Cache->VisibleEntities.Contains(Entity))
                                        {
                                            Cache->VisibleEntities.Add(Entity);
                                            Cache->EnteredEntities.Add(Entity);
                                        }
                                    }
                                }
                            }
                        }

                        // 구독 해제된 셀의 엔티티 제거 (OldCells - NewCells)
                        for (const FIntPoint& Cell : OldCells)
                        {
                            if (!Cache->SubscribedCellSet.Contains(Cell))
                            {
                                if (const TSet<FHktEntityId>* CellEntities = Stash->GetEntitiesInCell(Cell))
                                {
                                    for (FHktEntityId Entity : *CellEntities)
                                    {
                                        if (Cache->VisibleEntities.Remove(Entity) > 0)
                                        {
                                            Cache->ExitedEntities.Add(Entity);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Cache->LastLocation = CurrentLocation;
        }
    }

    // 4. 새 클라이언트 초기화 (구독 셀 내 모든 엔티티 스냅샷)
    for (AHktPlayerController* PC : NewClients)
    {
        if (FHktPlayerGridCache* Cache = PlayerCaches.Find(PC))
        {
            InitializeClientRelevancy(PC, *Cache);
        }
    }
    NewClients.Empty();

    // 5. 엔티티 셀 변경 이벤트 처리
    ProcessCellChangeEvents();

    // 6. 무효 캐시 정리
    TArray<AHktPlayerController*> ToRemove;
    for (auto& Pair : PlayerCaches)
    {
        if (!ValidClients.Contains(Pair.Key))
        {
            ToRemove.Add(Pair.Key);
        }
    }
    for (AHktPlayerController* PC : ToRemove)
    {
        PlayerCaches.Remove(PC);
    }
}

void UHktGridRelevancyComponent::UpdatePlayerSubscription(AHktPlayerController* PC, FHktPlayerGridCache& Cache)
{
    FVector Location = GetPlayerLocation(PC);
    FIntPoint NewCenterCell = LocationToCell(Location);

    // 새 구독 셀 계산
    Cache.SubscribedCellSet.Empty();
    for (int32 X = -InterestRadius; X <= InterestRadius; ++X)
    {
        for (int32 Y = -InterestRadius; Y <= InterestRadius; ++Y)
        {
            Cache.SubscribedCellSet.Add(FIntPoint(NewCenterCell.X + X, NewCenterCell.Y + Y));
        }
    }

    Cache.CurrentCell = NewCenterCell;
}

void UHktGridRelevancyComponent::InitializeClientRelevancy(AHktPlayerController* PC, FHktPlayerGridCache& Cache)
{
    // 구독 셀 초기화
    UpdatePlayerSubscription(PC, Cache);

    if (!MasterStash.IsValid())
    {
        return;
    }

    IHktMasterStashInterface* Stash = MasterStash->GetStash();
    if (!Stash)
    {
        return;
    }

    // 구독 셀 내 모든 엔티티를 Entered로 설정
    TSet<FHktEntityId> InitialEntities;
    Stash->GetEntitiesInCells(Cache.SubscribedCellSet, InitialEntities);

    for (FHktEntityId Entity : InitialEntities)
    {
        Cache.VisibleEntities.Add(Entity);
        Cache.EnteredEntities.Add(Entity);
    }

    UE_LOG(LogTemp, Log, TEXT("GridRelevancy: Initialized client %s with %d entities"),
        *PC->GetName(), InitialEntities.Num());
}

void UHktGridRelevancyComponent::ProcessCellChangeEvents()
{
    if (!MasterStash.IsValid())
    {
        return;
    }

    IHktMasterStashInterface* Stash = MasterStash->GetStash();
    if (!Stash)
    {
        return;
    }

    // MasterStash에서 셀 변경 이벤트 가져오기
    TArray<FHktCellChangeEvent> Events = Stash->ConsumeCellChangeEvents();

    for (const FHktCellChangeEvent& Event : Events)
    {
        // 각 클라이언트별로 처리
        for (AHktPlayerController* PC : ValidClients)
        {
            FHktPlayerGridCache* Cache = PlayerCaches.Find(PC);
            if (!Cache)
            {
                continue;
            }

            bool bWasVisible = (Event.OldCell != InvalidCell) && Cache->SubscribedCellSet.Contains(Event.OldCell);
            bool bIsVisible = (Event.NewCell != InvalidCell) && Cache->SubscribedCellSet.Contains(Event.NewCell);

            if (!bWasVisible && bIsVisible)
            {
                // 새로 보이게 됨 (Enter)
                if (!Cache->VisibleEntities.Contains(Event.Entity))
                {
                    Cache->VisibleEntities.Add(Event.Entity);
                    Cache->EnteredEntities.Add(Event.Entity);
                }
            }
            else if (bWasVisible && !bIsVisible)
            {
                // 더 이상 안 보임 (Exit)
                if (Cache->VisibleEntities.Remove(Event.Entity) > 0)
                {
                    Cache->ExitedEntities.Add(Event.Entity);
                }
            }
            // bWasVisible && bIsVisible: 여전히 보임 (같은 구독 영역 내 이동) → 변경 없음
        }
    }
}

// === Relevancy 조회 ===

FIntPoint UHktGridRelevancyComponent::LocationToCell(const FVector& Location) const
{
    return FIntPoint(
        FMath::FloorToInt(Location.X / CellSize),
        FMath::FloorToInt(Location.Y / CellSize)
    );
}

bool UHktGridRelevancyComponent::IsClientInterestedInCell(AHktPlayerController* Client, FIntPoint Cell) const
{
    if (const FHktPlayerGridCache* Cache = PlayerCaches.Find(Client))
    {
        return Cache->SubscribedCellSet.Contains(Cell);
    }
    return false;
}

void UHktGridRelevancyComponent::GetRelevantClientsAtLocation(
    const FVector& Location,
    TArray<AHktPlayerController*>& OutRelevantClients)
{
    OutRelevantClients.Reset();

    FIntPoint Cell = LocationToCell(Location);

    for (AHktPlayerController* PC : ValidClients)
    {
        if (IsClientInterestedInCell(PC, Cell))
        {
            OutRelevantClients.Add(PC);
        }
    }
}

// === 엔티티 Relevancy 조회 ===

TArray<FHktEntityId> UHktGridRelevancyComponent::GetEntitiesInRelevancy(AHktPlayerController* Client) const
{
    TArray<FHktEntityId> Result;

    if (const FHktPlayerGridCache* Cache = PlayerCaches.Find(Client))
    {
        Result = Cache->VisibleEntities.Array();
    }

    return Result;
}

TArray<FHktEntityId> UHktGridRelevancyComponent::GetNewlyVisibleEntities(AHktPlayerController* Client) const
{
    if (const FHktPlayerGridCache* Cache = PlayerCaches.Find(Client))
    {
        return Cache->EnteredEntities;
    }
    return TArray<FHktEntityId>();
}

TArray<FHktEntityId> UHktGridRelevancyComponent::GetRemovedEntities(AHktPlayerController* Client) const
{
    if (const FHktPlayerGridCache* Cache = PlayerCaches.Find(Client))
    {
        return Cache->ExitedEntities;
    }
    return TArray<FHktEntityId>();
}

// === 헬퍼 ===

FVector UHktGridRelevancyComponent::GetPlayerLocation(AHktPlayerController* PC) const
{
    if (!PC)
    {
        return FVector::ZeroVector;
    }

    if (AActor* ViewTarget = PC->GetViewTarget())
    {
        return ViewTarget->GetActorLocation();
    }

    if (APawn* Pawn = PC->GetPawn())
    {
        return Pawn->GetActorLocation();
    }

    return FVector::ZeroVector;
}
