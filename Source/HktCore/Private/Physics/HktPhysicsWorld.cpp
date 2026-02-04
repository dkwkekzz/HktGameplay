// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPhysicsWorld.h"
#include "HktCollisionTests.h"
#include "HktCoreInterfaces.h"

// ============================================================================
// 생성자 / 소멸자
// ============================================================================

FHktPhysicsWorld::FHktPhysicsWorld()
{
    ActiveColliders.Reserve(HktPhysics::MaxColliders);
    WatchedEntities.Reserve(64);
}

FHktPhysicsWorld::~FHktPhysicsWorld()
{
    Shutdown();
}

// ============================================================================
// 초기화 / 종료
// ============================================================================

void FHktPhysicsWorld::Initialize(IHktStashInterface* InStash)
{
    Stash = InStash;
    bActiveCollidersDirty = true;

    UE_LOG(LogTemp, Log, TEXT("[PhysicsWorld] Initialized"));
}

void FHktPhysicsWorld::Shutdown()
{
    WatchedEntities.Empty();
    ActiveColliders.Empty();
    Stash = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[PhysicsWorld] Shutdown"));
}

// ============================================================================
// Watch 기반 충돌 감지
// ============================================================================

int32 FHktPhysicsWorld::DetectWatchedCollisions(TArray<FHktCollisionPair>& OutCollisions)
{
    if (!Stash || WatchedEntities.Num() == 0)
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

        // Watch 엔티티 정보 캐싱
        const FVector WatchedPos = GetEntityPosition(WatchedEntity);
        const EHktColliderType WatchedType = GetColliderType(WatchedEntity);
        const float WatchedRadius = GetColliderRadius(WatchedEntity);
        const float WatchedHalfHeight = (WatchedType == EHktColliderType::Capsule)
            ? GetCapsuleHalfHeight(WatchedEntity) : 0.f;

        // 모든 활성 충돌체와 비교
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

                UE_LOG(LogTemp, Verbose, TEXT("[PhysicsWorld] Collision: %d <-> %d"),
                    WatchedEntity.RawValue, OtherEntity.RawValue);

                // 첫 번째 충돌만 보고
                break;
            }
        }
    }

    return CollisionCount;
}

int32 FHktPhysicsWorld::DetectAllCollisions(TArray<FHktCollisionPair>& OutPairs)
{
    if (!Stash)
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

void FHktPhysicsWorld::AddWatchedEntity(FHktEntityId Entity)
{
    if (Entity != InvalidEntityId)
    {
        WatchedEntities.Add(Entity);
        UE_LOG(LogTemp, Verbose, TEXT("[PhysicsWorld] AddWatched: %d"), Entity.RawValue);
    }
}

void FHktPhysicsWorld::RemoveWatchedEntity(FHktEntityId Entity)
{
    WatchedEntities.Remove(Entity);
    UE_LOG(LogTemp, Verbose, TEXT("[PhysicsWorld] RemoveWatched: %d"), Entity.RawValue);
}

bool FHktPhysicsWorld::IsWatched(FHktEntityId Entity) const
{
    return WatchedEntities.Contains(Entity);
}

void FHktPhysicsWorld::ClearWatchedEntities()
{
    WatchedEntities.Empty();
}

// ============================================================================
// 쿼리 API
// ============================================================================

int32 FHktPhysicsWorld::OverlapSphere(
    const FVector& Center,
    float Radius,
    TArray<FHktEntityId>& OutEntities,
    uint8 LayerMask,
    FHktEntityId ExcludeEntity) const
{
    if (!Stash)
    {
        return 0;
    }
    
    RefreshActiveColliders();
    
    int32 FoundCount = 0;
    
    for (FHktEntityId Entity : ActiveColliders)
    {
        if (Entity == ExcludeEntity)
        {
            continue;
        }
        
        if (!PassesLayerFilter(Entity, LayerMask))
        {
            continue;
        }
        
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
            {
                break;
            }
        }
    }
    
    return FoundCount;
}

int32 FHktPhysicsWorld::OverlapSphereReset(
    const FVector& Center,
    float Radius,
    TArray<FHktEntityId>& OutEntities,
    uint8 LayerMask,
    FHktEntityId ExcludeEntity) const
{
    OutEntities.Reset();
    return OverlapSphere(Center, Radius, OutEntities, LayerMask, ExcludeEntity);
}

bool FHktPhysicsWorld::Raycast(
    const FVector& Origin,
    const FVector& Direction,
    float MaxDistance,
    FHktRaycastResult& OutResult,
    uint8 LayerMask,
    FHktEntityId ExcludeEntity) const
{
    if (!Stash)
    {
        return false;
    }
    
    RefreshActiveColliders();
    
    OutResult.Reset();
    float BestDistance = MaxDistance;
    
    for (FHktEntityId Entity : ActiveColliders)
    {
        if (Entity == ExcludeEntity)
        {
            continue;
        }
        
        if (!PassesLayerFilter(Entity, LayerMask))
        {
            continue;
        }
        
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

bool FHktPhysicsWorld::SweepSphere(
    const FVector& Start,
    const FVector& End,
    float Radius,
    FHktSweepResult& OutResult,
    uint8 LayerMask,
    FHktEntityId ExcludeEntity) const
{
    if (!Stash)
    {
        return false;
    }
    
    RefreshActiveColliders();
    
    OutResult.Reset();
    float BestTime = 1.f;
    
    for (FHktEntityId Entity : ActiveColliders)
    {
        if (Entity == ExcludeEntity)
        {
            continue;
        }
        
        if (!PassesLayerFilter(Entity, LayerMask))
        {
            continue;
        }
        
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

bool FHktPhysicsWorld::TestEntityOverlap(FHktEntityId EntityA, FHktEntityId EntityB) const
{
    if (!Stash || !IsValidCollider(EntityA) || !IsValidCollider(EntityB))
    {
        return false;
    }
    
    if (!CanCollide(EntityA, EntityB))
    {
        return false;
    }
    
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

bool FHktPhysicsWorld::TestEntityCollision(
    FHktEntityId EntityA,
    FHktEntityId EntityB,
    FHktCollisionResult& OutResult) const
{
    OutResult.Reset();
    
    if (!Stash || !IsValidCollider(EntityA) || !IsValidCollider(EntityB))
    {
        return false;
    }
    
    if (!CanCollide(EntityA, EntityB))
    {
        return false;
    }
    
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

EHktColliderType FHktPhysicsWorld::GetColliderType(FHktEntityId Entity) const
{
    if (!Stash || !Stash->IsValidEntity(Entity))
    {
        return EHktColliderType::None;
    }
    return static_cast<EHktColliderType>(Stash->GetProperty(Entity, PropertyId::ColliderType));
}

uint8 FHktPhysicsWorld::GetCollisionLayer(FHktEntityId Entity) const
{
    if (!Stash || !Stash->IsValidEntity(Entity))
    {
        return 0;
    }
    return static_cast<uint8>(Stash->GetProperty(Entity, PropertyId::CollisionLayer));
}

uint8 FHktPhysicsWorld::GetCollisionMask(FHktEntityId Entity) const
{
    if (!Stash || !Stash->IsValidEntity(Entity))
    {
        return 0;
    }
    return static_cast<uint8>(Stash->GetProperty(Entity, PropertyId::CollisionMask));
}

bool FHktPhysicsWorld::IsValidCollider(FHktEntityId Entity) const
{
    return Stash && Stash->IsValidEntity(Entity) && 
           GetColliderType(Entity) != EHktColliderType::None;
}

bool FHktPhysicsWorld::CanCollide(FHktEntityId EntityA, FHktEntityId EntityB) const
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

FVector FHktPhysicsWorld::GetEntityPosition(FHktEntityId Entity) const
{
    return FVector(
        static_cast<float>(Stash->GetProperty(Entity, PropertyId::PosX)),
        static_cast<float>(Stash->GetProperty(Entity, PropertyId::PosY)),
        static_cast<float>(Stash->GetProperty(Entity, PropertyId::PosZ))
    );
}

float FHktPhysicsWorld::GetColliderRadius(FHktEntityId Entity) const
{
    return static_cast<float>(Stash->GetProperty(Entity, PropertyId::ColliderRadius));
}

float FHktPhysicsWorld::GetCapsuleHalfHeight(FHktEntityId Entity) const
{
    return static_cast<float>(Stash->GetProperty(Entity, PropertyId::ColliderHalfHeight));
}

void FHktPhysicsWorld::GetCapsuleEndpoints(FHktEntityId Entity, FVector& OutTop, FVector& OutBottom) const
{
    const FVector Center = GetEntityPosition(Entity);
    const float HalfHeight = GetCapsuleHalfHeight(Entity);
    
    OutTop = Center + FVector(0.f, 0.f, HalfHeight);
    OutBottom = Center - FVector(0.f, 0.f, HalfHeight);
}

bool FHktPhysicsWorld::PassesLayerFilter(FHktEntityId Entity, uint8 LayerMask) const
{
    return (GetCollisionLayer(Entity) & LayerMask) != 0;
}

void FHktPhysicsWorld::RefreshActiveColliders() const
{
    if (!bActiveCollidersDirty || !Stash)
    {
        return;
    }
    
    ActiveColliders.Reset();
    
    Stash->ForEachEntity([this](FHktEntityId Entity)
    {
        if (GetColliderType(Entity) != EHktColliderType::None)
        {
            ActiveColliders.Add(Entity);
        }
    });
    
    bActiveCollidersDirty = false;
    
    UE_LOG(LogTemp, Verbose, TEXT("[PhysicsWorld] RefreshActiveColliders: %d"), ActiveColliders.Num());
}

// ============================================================================
// 디버그
// ============================================================================

#if !UE_BUILD_SHIPPING

int32 FHktPhysicsWorld::GetActiveColliderCount() const
{
    RefreshActiveColliders();
    return ActiveColliders.Num();
}

FString FHktPhysicsWorld::GetDebugString() const
{
    RefreshActiveColliders();
    return FString::Printf(TEXT("[PhysicsWorld] Active=%d, Watched=%d"),
        ActiveColliders.Num(), WatchedEntities.Num());
}

FString FHktPhysicsWorld::GetColliderDebugString(FHktEntityId Entity) const
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
