// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktStash.h"
#include "HktCoreInterfaces.h"

/**
 * FHktMasterStash - 서버 전용 Stash 구현
 * 
 * 모든 엔티티의 "진실의 원천(Source of Truth)"
 * 스냅샷 생성, 변경 추적, 위치 관리 기능 제공
 */
class HKTCORE_API FHktMasterStash : public FHktStashBase, public IHktMasterStashInterface
{
public:
    FHktMasterStash();
    virtual ~FHktMasterStash() override = default;

    // ========== IHktStashInterface Implementation ==========
    virtual FHktEntityId AllocateEntity() override;
    virtual void FreeEntity(FHktEntityId Entity) override;
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

    // ========== IHktMasterStashInterface Implementation ==========
    virtual void ApplyWrites(const TArray<FPendingWrite>& Writes) override;
    virtual bool ValidateEntityFrame(FHktEntityId Entity, int32 FrameNumber) const override;
    virtual FHktEntitySnapshot CreateEntitySnapshot(FHktEntityId Entity) const override;
    virtual TArray<FHktEntitySnapshot> CreateSnapshots(const TArray<FHktEntityId>& Entities) const override;
    virtual TArray<uint8> SerializeFullState() const override;
    virtual void DeserializeFullState(const TArray<uint8>& Data) override;
    virtual bool TryGetPosition(FHktEntityId Entity, FVector& OutPosition) const override;
    virtual void SetPosition(FHktEntityId Entity, const FVector& Position) override;
    virtual uint32 CalculatePartialChecksum(const TArray<FHktEntityId>& Entities) const override;
    virtual void ForEachEntityInRadius(FHktEntityId Center, int32 RadiusCm, TFunctionRef<void(FHktEntityId)> Callback) const override;

    // ========== Cell-based Spatial Indexing ==========
    virtual void SetCellSize(float InCellSize) override;
    virtual float GetCellSize() const override { return CellSize; }
    virtual FIntPoint GetEntityCell(FHktEntityId Entity) const override;
    virtual const TSet<FHktEntityId>* GetEntitiesInCell(FIntPoint Cell) const override;
    virtual TArray<FHktCellChangeEvent> ConsumeCellChangeEvents() override;
    virtual void GetEntitiesInCells(const TSet<FIntPoint>& Cells, TSet<FHktEntityId>& OutEntities) const override;

private:
    /** 위치 → 셀 변환 */
    FIntPoint PositionToCell(const FVector& Position) const;

    /** 엔티티의 셀 변경 처리 (내부용) */
    void UpdateEntityCell(FHktEntityId Entity, FIntPoint NewCell);

    /** 엔티티 생성 프레임 (Validation용) */
    TArray<int32> EntityCreationFrame;

    // ========== Cell Spatial Index ==========

    /** 셀 크기 (cm 단위, 기본 5000 = 50m) */
    float CellSize = 5000.0f;

    /** 셀 → 엔티티 Set 매핑 */
    TMap<FIntPoint, TSet<FHktEntityId>> CellToEntities;

    /** 엔티티 → 현재 셀 매핑 */
    TArray<FIntPoint> EntityCells;

    /** 이번 프레임의 셀 변경 이벤트 */
    TArray<FHktCellChangeEvent> PendingCellChangeEvents;
};
