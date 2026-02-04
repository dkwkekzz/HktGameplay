// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVisibleStash.h"

FHktVisibleStash::FHktVisibleStash()
    : FHktStashBase()
{
    // VisibleStash는 스냅샷 적용 시 자동으로 엔티티를 생성함
    bAutoCreateOnSet = true;
}

void FHktVisibleStash::ApplyWrites(const TArray<FPendingWrite>& Writes)
{
    for (const auto& W : Writes)
    {
        SetProperty(W.Entity, W.PropertyId, W.Value);
    }
}

void FHktVisibleStash::ApplyEntitySnapshot(const FHktEntitySnapshot& Snapshot)
{
    if (!Snapshot.IsValid())
        return;
    
    FHktEntityId E = Snapshot.GetEntityId();
    
    if (E.RawValue < 0 || E.RawValue >= MaxEntities)
        return;
    
    // 엔티티 활성화
    ValidEntities[E.RawValue] = true;
    if (E.RawValue >= NextEntityId)
        NextEntityId = E.RawValue + 1;
    
    // Property 복사
    int32 NumProps = FMath::Min(Snapshot.Properties.Num(), MaxProperties);
    for (int32 PropId = 0; PropId < NumProps; ++PropId)
    {
        Properties[PropId][E.RawValue] = Snapshot.Properties[PropId];
    }
    
    // Tag 복사
    EntityTags[E.RawValue] = Snapshot.Tags;
    
    UE_LOG(LogTemp, Verbose, TEXT("[VisibleStash] Applied snapshot for Entity %d (Tags: %d)"), 
        E.RawValue, Snapshot.Tags.Num());
}

void FHktVisibleStash::ApplySnapshots(const TArray<FHktEntitySnapshot>& Snapshots)
{
    for (const FHktEntitySnapshot& Snap : Snapshots)
    {
        ApplyEntitySnapshot(Snap);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[VisibleStash] Applied %d snapshots"), Snapshots.Num());
}

void FHktVisibleStash::Clear()
{
    ValidEntities.Init(false, MaxEntities);
    FreeList.Reset();
    NextEntityId = 0;
    CompletedFrameNumber = 0;
    
    for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
    {
        FMemory::Memzero(Properties[PropId].GetData(), MaxEntities * sizeof(int32));
    }
    
    for (int32 E = 0; E < MaxEntities; ++E)
    {
        EntityTags[E].Reset();
    }
}
