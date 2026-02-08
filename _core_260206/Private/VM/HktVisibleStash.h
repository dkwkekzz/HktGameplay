// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktStash.h"
#include "HktCoreInterfaces.h"

/**
 * FHktVisibleStash - 클라이언트 전용 Stash 구현
 * 
 * IntentEvent를 받아 결정론적으로 실행한 결과를 저장
 * 서버와 동일한 입력 → 동일한 결과 보장
 */
class HKTCORE_API FHktVisibleStash : public FHktStashBase, public IHktVisibleStashInterface
{
public:
    FHktVisibleStash();
    virtual ~FHktVisibleStash() override = default;

    // ========== IHktStashInterface Implementation ==========
    virtual FHktEntityId AllocateEntity() override { return FHktStashBase::AllocateEntity(); }
    virtual void FreeEntity(FHktEntityId Entity) override { FHktStashBase::FreeEntity(Entity); }
    virtual bool IsValidEntity(FHktEntityId Entity) const override { return FHktStashBase::IsValidEntity(Entity); }
    virtual int32 GetProperty(FHktEntityId Entity, uint16 PropertyId) const override { return FHktStashBase::GetProperty(Entity, PropertyId); }
    virtual void SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value) override { FHktStashBase::SetProperty(Entity, PropertyId, Value); }
    virtual int32 GetEntityCount() const override { return FHktStashBase::GetEntityCount(); }
    virtual int32 GetCompletedFrameNumber() const override { return FHktStashBase::GetCompletedFrameNumber(); }
    virtual void MarkFrameCompleted(int32 FrameNumber) override { FHktStashBase::MarkFrameCompleted(FrameNumber); }
    virtual void ForEachEntity(TFunctionRef<void(FHktEntityId)> Callback) const override { FHktStashBase::ForEachEntity(Callback); }
    virtual uint32 CalculateChecksum() const override { return FHktStashBase::CalculateChecksum(); }

    // ========== Tag API Implementation ==========
    virtual const FGameplayTagContainer& GetTags(FHktEntityId Entity) const override { return FHktStashBase::GetTags(Entity); }
    virtual void SetTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) override { FHktStashBase::SetTags(Entity, Tags); }
    virtual void AddTag(FHktEntityId Entity, const FGameplayTag& Tag) override { FHktStashBase::AddTag(Entity, Tag); }
    virtual void RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag) override { FHktStashBase::RemoveTag(Entity, Tag); }
    virtual bool HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const override { return FHktStashBase::HasTag(Entity, Tag); }
    virtual bool HasTagExact(FHktEntityId Entity, const FGameplayTag& Tag) const override { return FHktStashBase::HasTagExact(Entity, Tag); }
    virtual bool HasAnyTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const override { return FHktStashBase::HasAnyTags(Entity, Tags); }
    virtual bool HasAllTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const override { return FHktStashBase::HasAllTags(Entity, Tags); }
    virtual FGameplayTag GetFirstTagWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const override { return FHktStashBase::GetFirstTagWithParent(Entity, ParentTag); }
    virtual FGameplayTagContainer GetTagsWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const override { return FHktStashBase::GetTagsWithParent(Entity, ParentTag); }

    // ========== IHktVisibleStashInterface Implementation ==========
    virtual void ApplyWrites(const TArray<FPendingWrite>& Writes) override;
    virtual void ApplyEntitySnapshot(const FHktEntitySnapshot& Snapshot) override;
    virtual void ApplySnapshots(const TArray<FHktEntitySnapshot>& Snapshots) override;
    virtual void Clear() override;
};
