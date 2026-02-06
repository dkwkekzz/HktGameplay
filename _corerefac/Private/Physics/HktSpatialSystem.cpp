// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Physics/HktSpatialSystem.h"
#include "State/HktWorldState.h"
#include "State/HktComponentTypes.h"
#include "HktCollisionTests.h"

// ============================================================================
// 생성자 / 소멸자
// ============================================================================

FHktSpatialSystem::FHktSpatialSystem()
{
    ActiveColliders.Reserve(HktPhysics::MaxColliders);
    WatchedEntities.Reserve(64);
    EntityCells.Init(InvalidCell, HktCore::MaxEntities);
}

FHktSpatialSystem::~FHktSpatialSystem()
{
    Shutdown();
}

// ============================================================================
// 초기화 / 종료
// ============================================================================

void FHktSpatialSystem::Initialize(FHktWorldState* InWorldState)
{
    WorldState = InWorldState;
    bActiveCollidersDirty = true;

    UE_LOG(LogTemp, Log, TEXT("[SpatialSystem] Initialized"));
}

void FHktSpatialSystem::Shutdown()
{
    WatchedEntities.Empty();
    ActiveColliders.Empty();
    CellToEntities.Empty();
    PendingCellChangeEvents.Empty();
    WorldState = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[SpatialSystem] Shutdown"));
}

// ============================================================================
// Cell 기반 공간 관리
// ============================================================================

void FHktSpatialSystem::SetCellSize(float InCellSize)
{
    if (InCellSize > 0.0f && InCellSize != CellSize)
    {
        CellSize = InCellSize;

        // 셀 인덱스 재구축
        CellToEntities.Empty();
        PendingCellChangeEvents.Empty();

        if (WorldState)
        {
            WorldState->ForEachEntity([this](FHktEntityId Entity)
            {
                FVector Pos;
                if (WorldState->TryGetPosition(Entity, Pos))
                {
                    FIntPoint NewCell = PositionToCell(Pos);
                    EntityCells[Entity.RawValue] = NewCell;
                    CellToEntities.FindOrAdd(NewCell).Add(Entity);
                }
            });
        }

        UE_LOG(LogTemp, Log, TEXT("[SpatialSystem] CellSize changed to %.0f, rebuilt spatial index"), CellSize);
    }
}

FIntPoint FHktSpatialSystem::GetEntityCell(FHktEntityId Entity) const
{
    if (!WorldState || !WorldState->IsValidEntity(Entity))
    {
        return InvalidCell;
    }
    return EntityCells[Entity.RawValue];
}

const TSet<FHktEntityId>* FHktSpatialSystem::GetEntitiesInCell(FIntPoint Cell) const
{
    return CellToEntities.Find(Cell);
}

TArray<FHktCellChangeEvent> FHktSpatialSystem::ConsumeCellChangeEvents()
{
    TArray<FHktCellChangeEvent> Result = MoveTemp(PendingCellChangeEvents);
    PendingCellChangeEvents.Empty();
    return Result;
}

void FHktSpatialSystem::GetEntitiesInCells(const TSet<FIntPoint>& Cells, TSet<FHktEntityId>& OutEntities) const
{
    for (const FIntPoint& Cell : Cells)
    {
        if (const TSet<FHktEntityId>* CellEntities = CellToEntities.Find(Cell))
        {
            OutEntities.Append(*CellEntities);
        }
    }
}

void FHktSpatialSystem::UpdateEntityPositions()
{
    if (!WorldState)
        return;

    WorldState->ForEachEntity([this](FHktEntityId Entity)
    {
        FVector Pos;
        if (WorldState->TryGetPosition(Entity, Pos))
        {
            FIntPoint NewCell = PositionToCell(Pos);
            UpdateEntityCell(Entity, NewCell);
        }
    });

    // 위치 변경이 있을 수 있으므로 Active 충돌체 갱신 필요
    bActiveCollidersDirty = true;
}

void FHktSpatialSystem::OnEntityAllocated(FHktEntityId Entity)
{
    if (Entity != InvalidEntityId && Entity.RawValue < HktCore::MaxEntities)
    {
        EntityCells[Entity.RawValue] = InvalidCell;
        bActiveCollidersDirty = true;
    }
}

void FHktSpatialSystem::OnEntityFreed(FHktEntityId Entity)
{
    if (Entity.RawValue < 0 || Entity.RawValue >= HktCore::MaxEntities)
        return;

    FIntPoint OldCell = EntityCells[Entity.RawValue];

    // 셀에서 제거
    if (OldCell != InvalidCell)
    {
        if (TSet<FHktEntityId>* CellSet = CellToEntities.Find(OldCell))
        {
            CellSet->Remove(Entity);
            if (CellSet->Num() == 0)
            {
                CellToEntities.Remove(OldCell);
            }
        }

        // Exit 이벤트 발생
        FHktCellChangeEvent Event;
        Event.Entity = Entity;
        Event.OldCell = OldCell;
        Event.NewCell = InvalidCell;
        PendingCellChangeEvents.Add(Event);
    }

    EntityCells[Entity.RawValue] = InvalidCell;
    WatchedEntities.Remove(Entity);
    bActiveCollidersDirty = true;
}

FIntPoint FHktSpatialSystem::PositionToCell(const FVector& Position) const
{
    return FIntPoint(
        FMath::FloorToInt(Position.X / CellSize),
        FMath::FloorToInt(Position.Y / CellSize)
    );
}

void FHktSpatialSystem::UpdateEntityCell(FHktEntityId Entity, FIntPoint NewCell)
{
    FIntPoint OldCell = EntityCells[Entity.RawValue];

    if (OldCell == NewCell)
    {
        return;
    }

    // 이전 셀에서 제거
    if (OldCell != InvalidCell)
    {
        if (TSet<FHktEntityId>* CellSet = CellToEntities.Find(OldCell))
        {
            CellSet->Remove(Entity);
            if (CellSet->Num() == 0)
            {
                CellToEntities.Remove(OldCell);
            }
        }
    }

    // 새 셀에 추가
    if (NewCell != InvalidCell)
    {
        CellToEntities.FindOrAdd(NewCell).Add(Entity);
    }

    // 셀 업데이트
    EntityCells[Entity.RawValue] = NewCell;

    // 변경 이벤트 발생
    FHktCellChangeEvent Event;
    Event.Entity = Entity;
    Event.OldCell = OldCell;
    Event.NewCell = NewCell;
    PendingCellChangeEvents.Add(Event);
}

// ============================================================================
// Resolve Now, React Later - 충돌 해결 및 이벤트 생성
// ============================================================================

int32 FHktSpatialSystem::ResolveOverlapsAndGenEvents(
    FHktWorldState& State,
    TArray<FHktSystemEvent>& OutEvents)
{
    TArray<FHktCollisionPair> Collisions;
    DetectWatchedCollisions(Collisions);

    int32 ResolvedCount = 0;

    for (const FHktCollisionPair& Pair : Collisions)
    {
        FHktCollisionResult Result;
        if (!TestEntityCollision(Pair.EntityA, Pair.EntityB, Result))
        {
            continue;
        }

        // ============================================================
        // [Immediate] Position Depenetration
        // 수학적으로 즉시 분리 → 시각적 떨림 방지
        // ============================================================
        FVector PosA, PosB;
        if (State.TryGetPosition(Pair.EntityA, PosA) &&
            State.TryGetPosition(Pair.EntityB, PosB))
        {
            const float HalfPenetration = Result.PenetrationDepth * 0.5f;
            PosA -= Result.ContactNormal * HalfPenetration;
            PosB += Result.ContactNormal * HalfPenetration;

            State.SetPosition(Pair.EntityA, PosA);
            State.SetPosition(Pair.EntityB, PosB);

            UE_LOG(LogTemp, Verbose, TEXT("[SpatialSystem] Depenetrated: %d <-> %d, Depth=%.2f"),
                Pair.EntityA.RawValue, Pair.EntityB.RawValue, Result.PenetrationDepth);
        }

        // ============================================================
        // [Deferred] Generate Collision SystemEvent
        // 게임플레이 로직(HP 감소, 사망 등)은 다음 프레임에 처리
        // ============================================================
        if (CollisionEventTag.IsValid())
        {
            FHktSystemEvent CollisionEvent;
            CollisionEvent.EventTag = CollisionEventTag;
            CollisionEvent.SourceEntity = Pair.EntityA;
            CollisionEvent.TargetEntity = Pair.EntityB;
            CollisionEvent.Location = Result.ContactPoint;
            OutEvents.Add(CollisionEvent);
        }

        ++ResolvedCount;
    }

    return ResolvedCount;
}

// ============================================================================
// Watch 기반 충돌 감지
// ============================================================================

int32 FHktSpatialSystem::DetectWatchedCollisions(TArray<FHktCollisionPair>& OutCollisions)
{
    if (!WorldState || WatchedEntities.Num() == 0)
    {
        return 0;
    }

    RefreshActiveColliders();

    int32 CollisionCount = 0;

    for (FHktEntityId WatchedEntity : WatchedEntities)
    {
        if (!IsValidCollider(WatchedEntity))
        {
            continue;
        }

        const FVector WatchedPos = GetEntityPosition(WatchedEntity);
        const EHktColliderType WatchedType = GetColliderType(WatchedEntity);
        const float WatchedRadius = GetColliderRadius(WatchedEntity);
        const float WatchedHalfHeight = (WatchedType == EHktColliderType::Capsule)
            ? GetCapsuleHalfHeight(WatchedEntity) : 0.f;

        for (FHktEntityId OtherEntity : ActiveColliders)
        {
            if (OtherEntity == WatchedEntity)
            {
                continue;
            }

            if (!CanCollide(WatchedEntity, OtherEntity))
            {
                continue;
            }

            const FVector OtherPos = GetEntityPosition(OtherEntity);
            const EHktColliderType OtherType = GetColliderType(OtherEntity);
            const float OtherRadius = GetColliderRadius(OtherEntity);
            const float OtherHalfHeight = (OtherType == EHktColliderType::Capsule)
                ? GetCapsuleHalfHeight(OtherEntity) : 0.f;

            if (HktPhysics::OverlapColliders(
                WatchedType, WatchedPos, WatchedRadius, WatchedHalfHeight,
                OtherType, OtherPos, OtherRadius, OtherHalfHeight))
            {
                FHktCollisionPair Pair;
                Pair.EntityA = WatchedEntity;
                Pair.EntityB = OtherEntity;
                OutCollisions.Add(Pair);
                ++CollisionCount;

                UE_LOG(LogTemp, Verbose, TEXT("[SpatialSystem] Collision: %d <-> %d"),
                    WatchedEntity.RawValue, OtherEntity.RawValue);

                break;
            }
        }
    }

    return CollisionCount;
}

int32 FHktSpatialSystem::DetectAllCollisions(TArray<FHktCollisionPair>& OutPairs)
{
    if (!WorldState)
    {
        return 0;
    }

    RefreshActiveColliders();

    const int32 NumColliders = ActiveColliders.Num();
    int32 PairCount = 0;

    for (int32 i = 0; i < NumColliders; ++i)
    {
        const FHktEntityId EntityA = ActiveColliders[i];
        const FVector PosA = GetEntityPosition(EntityA);
        const EHktColliderType TypeA = GetColliderType(EntityA);
        const float RadiusA = GetColliderRadius(EntityA);
        const float HalfHeightA = (TypeA == EHktColliderType::Capsule)
            ? GetCapsuleHalfHeight(EntityA) : 0.f;

        for (int32 j = i + 1; j < NumColliders; ++j)
        {
            const FHktEntityId EntityB = ActiveColliders[j];

            if (!CanCollide(EntityA, EntityB))
            {
                continue;
            }

            const FVector PosB = GetEntityPosition(EntityB);
            const EHktColliderType TypeB = GetColliderType(EntityB);
            const float RadiusB = GetColliderRadius(EntityB);
            const float HalfHeightB = (TypeB == EHktColliderType::Capsule)
                ? GetCapsuleHalfHeight(EntityB) : 0.f;

            if (HktPhysics::OverlapColliders(TypeA, PosA, RadiusA, HalfHeightA,
                                             TypeB, PosB, RadiusB, HalfHeightB))
            {
                FHktCollisionPair Pair;
                Pair.EntityA = EntityA;
                Pair.EntityB = EntityB;
                OutPairs.Add(Pair);
                ++PairCount;
            }
        }
    }

    return PairCount;
}

// ============================================================================
// Watch 관리
// ============================================================================

void FHktSpatialSystem::AddWatchedEntity(FHktEntityId Entity)
{
    if (Entity != InvalidEntityId)
    {
        WatchedEntities.Add(Entity);
    }
}

void FHktSpatialSystem::RemoveWatchedEntity(FHktEntityId Entity)
{
    WatchedEntities.Remove(Entity);
}

bool FHktSpatialSystem::IsWatched(FHktEntityId Entity) const
{
    return WatchedEntities.Contains(Entity);
}

void FHktSpatialSystem::ClearWatchedEntities()
{
    WatchedEntities.Empty();
}

// ============================================================================
// 쿼리 API
// ============================================================================

int32 FHktSpatialSystem::OverlapSphere(
    const FVector& Center,
    float Radius,
    TArray<FHktEntityId>& OutEntities,
    uint8 LayerMask,
    FHktEntityId ExcludeEntity) const
{
    if (!WorldState)
    {
        return 0;
    }

    RefreshActiveColliders();

    int32 FoundCount = 0;

    for (FHktEntityId Entity : ActiveColliders)
    {
        if (Entity == ExcludeEntity)
            continue;

        if (!PassesLayerFilter(Entity, LayerMask))
            continue;

        const FVector Pos = GetEntityPosition(Entity);
        const EHktColliderType Type = GetColliderType(Entity);
        const float ColliderRadius = GetColliderRadius(Entity);

        bool bOverlap = false;

        if (Type == EHktColliderType::Sphere)
        {
            bOverlap = HktPhysics::OverlapSphereSphere(Center, Radius, Pos, ColliderRadius);
        }
        else if (Type == EHktColliderType::Capsule)
        {
            FVector CapsuleTop, CapsuleBottom;
            GetCapsuleEndpoints(Entity, CapsuleTop, CapsuleBottom);
            bOverlap = HktPhysics::OverlapSphereCapsule(Center, Radius,
                CapsuleTop, CapsuleBottom, ColliderRadius);
        }

        if (bOverlap)
        {
            OutEntities.Add(Entity);
            ++FoundCount;

            if (FoundCount >= HktPhysics::MaxOverlapResults)
                break;
        }
    }

    return FoundCount;
}

int32 FHktSpatialSystem::OverlapSphereReset(
    const FVector& Center,
    float Radius,
    TArray<FHktEntityId>& OutEntities,
    uint8 LayerMask,
    FHktEntityId ExcludeEntity) const
{
    OutEntities.Reset();
    return OverlapSphere(Center, Radius, OutEntities, LayerMask, ExcludeEntity);
}

bool FHktSpatialSystem::Raycast(
    const FVector& Origin,
    const FVector& Direction,
    float MaxDistance,
    FHktRaycastResult& OutResult,
    uint8 LayerMask,
    FHktEntityId ExcludeEntity) const
{
    if (!WorldState)
        return false;

    RefreshActiveColliders();

    OutResult.Reset();
    float BestDistance = MaxDistance;

    for (FHktEntityId Entity : ActiveColliders)
    {
        if (Entity == ExcludeEntity)
            continue;

        if (!PassesLayerFilter(Entity, LayerMask))
            continue;

        const FVector Pos = GetEntityPosition(Entity);
        const EHktColliderType Type = GetColliderType(Entity);
        const float ColliderRadius = GetColliderRadius(Entity);

        float HitDistance = 0.f;
        FVector HitPoint = FVector::ZeroVector;
        FVector HitNormal = FVector::ZeroVector;
        bool bHit = false;

        if (Type == EHktColliderType::Sphere)
        {
            bHit = HktPhysics::RaycastSphere(Origin, Direction, BestDistance,
                Pos, ColliderRadius, HitDistance, HitPoint, HitNormal);
        }
        else if (Type == EHktColliderType::Capsule)
        {
            FVector CapsuleTop, CapsuleBottom;
            GetCapsuleEndpoints(Entity, CapsuleTop, CapsuleBottom);
            bHit = HktPhysics::RaycastCapsule(Origin, Direction, BestDistance,
                CapsuleTop, CapsuleBottom, ColliderRadius, HitDistance, HitPoint, HitNormal);
        }

        if (bHit && HitDistance < BestDistance)
        {
            BestDistance = HitDistance;
            OutResult.HitEntity = Entity;
            OutResult.Distance = HitDistance;
            OutResult.HitPoint = HitPoint;
            OutResult.HitNormal = HitNormal;
        }
    }

    return OutResult.IsValid();
}

bool FHktSpatialSystem::SweepSphere(
    const FVector& Start,
    const FVector& End,
    float Radius,
    FHktSweepResult& OutResult,
    uint8 LayerMask,
    FHktEntityId ExcludeEntity) const
{
    if (!WorldState)
        return false;

    RefreshActiveColliders();

    OutResult.Reset();
    float BestTime = 1.f;

    for (FHktEntityId Entity : ActiveColliders)
    {
        if (Entity == ExcludeEntity)
            continue;

        if (!PassesLayerFilter(Entity, LayerMask))
            continue;

        const FVector Pos = GetEntityPosition(Entity);
        const EHktColliderType Type = GetColliderType(Entity);
        const float ColliderRadius = GetColliderRadius(Entity);

        float HitTime = 0.f;
        FVector HitContact = FVector::ZeroVector;
        FVector HitNormal = FVector::ZeroVector;
        bool bHit = false;

        if (Type == EHktColliderType::Sphere)
        {
            bHit = HktPhysics::SweepSphereSphere(Start, End, Radius,
                Pos, ColliderRadius, HitTime, HitContact, HitNormal);
        }
        else if (Type == EHktColliderType::Capsule)
        {
            FVector CapsuleTop, CapsuleBottom;
            GetCapsuleEndpoints(Entity, CapsuleTop, CapsuleBottom);
            bHit = HktPhysics::SweepSphereCapsule(Start, End, Radius,
                CapsuleTop, CapsuleBottom, ColliderRadius, HitTime, HitContact, HitNormal);
        }

        if (bHit && HitTime < BestTime)
        {
            BestTime = HitTime;
            OutResult.HitEntity = Entity;
            OutResult.HitTime = HitTime;
            OutResult.HitPoint = HitContact;
            OutResult.HitNormal = HitNormal;
            OutResult.Distance = (End - Start).Size() * HitTime;
        }
    }

    return OutResult.IsValid();
}

bool FHktSpatialSystem::TestEntityOverlap(FHktEntityId EntityA, FHktEntityId EntityB) const
{
    if (!WorldState || !IsValidCollider(EntityA) || !IsValidCollider(EntityB))
        return false;

    if (!CanCollide(EntityA, EntityB))
        return false;

    const FVector PosA = GetEntityPosition(EntityA);
    const FVector PosB = GetEntityPosition(EntityB);
    const EHktColliderType TypeA = GetColliderType(EntityA);
    const EHktColliderType TypeB = GetColliderType(EntityB);
    const float RadiusA = GetColliderRadius(EntityA);
    const float RadiusB = GetColliderRadius(EntityB);
    const float HalfHeightA = (TypeA == EHktColliderType::Capsule) ? GetCapsuleHalfHeight(EntityA) : 0.f;
    const float HalfHeightB = (TypeB == EHktColliderType::Capsule) ? GetCapsuleHalfHeight(EntityB) : 0.f;

    return HktPhysics::OverlapColliders(TypeA, PosA, RadiusA, HalfHeightA,
                                        TypeB, PosB, RadiusB, HalfHeightB);
}

bool FHktSpatialSystem::TestEntityCollision(
    FHktEntityId EntityA,
    FHktEntityId EntityB,
    FHktCollisionResult& OutResult) const
{
    OutResult.Reset();

    if (!WorldState || !IsValidCollider(EntityA) || !IsValidCollider(EntityB))
        return false;

    if (!CanCollide(EntityA, EntityB))
        return false;

    const FVector PosA = GetEntityPosition(EntityA);
    const FVector PosB = GetEntityPosition(EntityB);
    const EHktColliderType TypeA = GetColliderType(EntityA);
    const EHktColliderType TypeB = GetColliderType(EntityB);
    const float RadiusA = GetColliderRadius(EntityA);
    const float RadiusB = GetColliderRadius(EntityB);
    const float HalfHeightA = (TypeA == EHktColliderType::Capsule) ? GetCapsuleHalfHeight(EntityA) : 0.f;
    const float HalfHeightB = (TypeB == EHktColliderType::Capsule) ? GetCapsuleHalfHeight(EntityB) : 0.f;

    FVector Contact, Normal;
    float Depth;

    if (HktPhysics::TestColliders(TypeA, PosA, RadiusA, HalfHeightA,
                                  TypeB, PosB, RadiusB, HalfHeightB,
                                  Contact, Normal, Depth))
    {
        OutResult.EntityA = EntityA;
        OutResult.EntityB = EntityB;
        OutResult.ContactPoint = Contact;
        OutResult.ContactNormal = Normal;
        OutResult.PenetrationDepth = Depth;
        return true;
    }

    return false;
}

// ============================================================================
// 충돌체 정보 조회
// ============================================================================

EHktColliderType FHktSpatialSystem::GetColliderType(FHktEntityId Entity) const
{
    if (!WorldState || !WorldState->IsValidEntity(Entity))
        return EHktColliderType::None;
    return static_cast<EHktColliderType>(WorldState->GetProperty(Entity, PropertyId::ColliderType));
}

uint8 FHktSpatialSystem::GetCollisionLayer(FHktEntityId Entity) const
{
    if (!WorldState || !WorldState->IsValidEntity(Entity))
        return 0;
    return static_cast<uint8>(WorldState->GetProperty(Entity, PropertyId::CollisionLayer));
}

uint8 FHktSpatialSystem::GetCollisionMask(FHktEntityId Entity) const
{
    if (!WorldState || !WorldState->IsValidEntity(Entity))
        return 0;
    return static_cast<uint8>(WorldState->GetProperty(Entity, PropertyId::CollisionMask));
}

bool FHktSpatialSystem::IsValidCollider(FHktEntityId Entity) const
{
    return WorldState && WorldState->IsValidEntity(Entity) &&
           GetColliderType(Entity) != EHktColliderType::None;
}

bool FHktSpatialSystem::CanCollide(FHktEntityId EntityA, FHktEntityId EntityB) const
{
    const uint8 LayerA = GetCollisionLayer(EntityA);
    const uint8 LayerB = GetCollisionLayer(EntityB);
    const uint8 MaskA = GetCollisionMask(EntityA);
    const uint8 MaskB = GetCollisionMask(EntityB);

    return (LayerA & MaskB) != 0 && (LayerB & MaskA) != 0;
}

// ============================================================================
// 내부 헬퍼
// ============================================================================

FVector FHktSpatialSystem::GetEntityPosition(FHktEntityId Entity) const
{
    return FVector(
        static_cast<float>(WorldState->GetProperty(Entity, PropertyId::PosX)),
        static_cast<float>(WorldState->GetProperty(Entity, PropertyId::PosY)),
        static_cast<float>(WorldState->GetProperty(Entity, PropertyId::PosZ))
    );
}

float FHktSpatialSystem::GetColliderRadius(FHktEntityId Entity) const
{
    return static_cast<float>(WorldState->GetProperty(Entity, PropertyId::ColliderRadius));
}

float FHktSpatialSystem::GetCapsuleHalfHeight(FHktEntityId Entity) const
{
    return static_cast<float>(WorldState->GetProperty(Entity, PropertyId::ColliderHalfHeight));
}

void FHktSpatialSystem::GetCapsuleEndpoints(FHktEntityId Entity, FVector& OutTop, FVector& OutBottom) const
{
    const FVector Center = GetEntityPosition(Entity);
    const float HalfHeight = GetCapsuleHalfHeight(Entity);

    OutTop = Center + FVector(0.f, 0.f, HalfHeight);
    OutBottom = Center - FVector(0.f, 0.f, HalfHeight);
}

bool FHktSpatialSystem::PassesLayerFilter(FHktEntityId Entity, uint8 LayerMask) const
{
    return (GetCollisionLayer(Entity) & LayerMask) != 0;
}

void FHktSpatialSystem::RefreshActiveColliders() const
{
    if (!bActiveCollidersDirty || !WorldState)
        return;

    ActiveColliders.Reset();

    WorldState->ForEachEntity([this](FHktEntityId Entity)
    {
        if (GetColliderType(Entity) != EHktColliderType::None)
        {
            ActiveColliders.Add(Entity);
        }
    });

    bActiveCollidersDirty = false;
}

// ============================================================================
// 디버그
// ============================================================================

#if !UE_BUILD_SHIPPING

int32 FHktSpatialSystem::GetActiveColliderCount() const
{
    RefreshActiveColliders();
    return ActiveColliders.Num();
}

FString FHktSpatialSystem::GetDebugString() const
{
    RefreshActiveColliders();
    return FString::Printf(TEXT("[SpatialSystem] Active=%d, Watched=%d, Cells=%d"),
        ActiveColliders.Num(), WatchedEntities.Num(), CellToEntities.Num());
}

FString FHktSpatialSystem::GetColliderDebugString(FHktEntityId Entity) const
{
    if (!IsValidCollider(Entity))
    {
        return FString::Printf(TEXT("Entity %d: Invalid/NoCollider"), Entity.RawValue);
    }

    const EHktColliderType Type = GetColliderType(Entity);
    const FVector Pos = GetEntityPosition(Entity);
    const float Radius = GetColliderRadius(Entity);
    const uint8 Layer = GetCollisionLayer(Entity);
    const uint8 Mask = GetCollisionMask(Entity);

    if (Type == EHktColliderType::Sphere)
    {
        return FString::Printf(
            TEXT("Entity %d: Sphere(R=%.1f) @ (%.0f,%.0f,%.0f) L=0x%02X M=0x%02X"),
            Entity.RawValue, Radius, Pos.X, Pos.Y, Pos.Z, Layer, Mask);
    }
    else
    {
        const float HalfHeight = GetCapsuleHalfHeight(Entity);
        return FString::Printf(
            TEXT("Entity %d: Capsule(HH=%.1f,R=%.1f) @ (%.0f,%.0f,%.0f) L=0x%02X M=0x%02X"),
            Entity.RawValue, HalfHeight, Radius, Pos.X, Pos.Y, Pos.Z, Layer, Mask);
    }
}

#endif
