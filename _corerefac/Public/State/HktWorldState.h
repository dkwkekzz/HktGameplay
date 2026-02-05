// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Common/HktTypes.h"
#include "Common/HktEvents.h"
#include "State/HktComponentTypes.h"

/**
 * FHktWorldState - 확정된 전역 상태 (Global Repository)
 *
 * 모든 엔티티의 확정된(Committed) 데이터 저장소.
 * 외부(HktUnreal)는 오직 이 객체를 통해 렌더링 데이터를 읽음.
 *
 * 내부적으로 SOA(Structure of Arrays) 레이아웃을 사용하여
 * 캐시 효율성과 결정론적 접근 보장.
 *
 * 이전 FHktStashBase + FHktMasterStash 통합 (셀 관리 제외 → SpatialSystem으로 이동)
 */
class HKTCORE_API FHktWorldState
{
public:
    FHktWorldState();
    virtual ~FHktWorldState() = default;

    // ========== Entity Management ==========
    FHktEntityId AllocateEntity();
    void FreeEntity(FHktEntityId Entity);
    bool IsValidEntity(FHktEntityId Entity) const;
    int32 GetEntityCount() const;

    // ========== Property API (숫자 데이터) ==========
    int32 GetProperty(FHktEntityId Entity, uint16 PropertyId) const;
    void SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value);

    // ========== Batch Write ==========
    struct FPendingWrite
    {
        FHktEntityId Entity;
        uint16 PropertyId;
        int32 Value;
    };
    void ApplyWrites(const TArray<FPendingWrite>& Writes);

    // ========== Tag API (GameplayTagContainer) ==========
    const FGameplayTagContainer& GetTags(FHktEntityId Entity) const;
    void SetTags(FHktEntityId Entity, const FGameplayTagContainer& Tags);
    void AddTag(FHktEntityId Entity, const FGameplayTag& Tag);
    void RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag);
    bool HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const;
    bool HasTagExact(FHktEntityId Entity, const FGameplayTag& Tag) const;
    bool HasAnyTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const;
    bool HasAllTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const;

    // ========== Tag Query Helpers ==========
    FGameplayTag GetFirstTagWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const;
    FGameplayTagContainer GetTagsWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const;

    // ========== Frame Management ==========
    int32 GetCompletedFrameNumber() const { return CompletedFrameNumber; }
    void MarkFrameCompleted(int32 FrameNumber);

    // ========== Iteration ==========
    void ForEachEntity(TFunctionRef<void(FHktEntityId)> Callback) const;

    // ========== Checksum ==========
    uint32 CalculateChecksum() const;
    uint32 CalculatePartialChecksum(const TArray<FHktEntityId>& Entities) const;

    // ========== Snapshot & Serialization ==========
    FHktEntitySnapshot CreateEntitySnapshot(FHktEntityId Entity) const;
    TArray<FHktEntitySnapshot> CreateSnapshots(const TArray<FHktEntityId>& Entities) const;
    TArray<uint8> SerializeFullState() const;
    void DeserializeFullState(const TArray<uint8>& Data);

    // ========== Position Access (Property 래핑) ==========
    bool TryGetPosition(FHktEntityId Entity, FVector& OutPosition) const;
    void SetPosition(FHktEntityId Entity, const FVector& Position);

    // ========== Radius Query ==========
    void ForEachEntityInRadius(FHktEntityId Center, int32 RadiusCm, TFunctionRef<void(FHktEntityId)> Callback) const;

    // ========== Frame Validation ==========
    bool ValidateEntityFrame(FHktEntityId Entity, int32 FrameNumber) const;

protected:
    static constexpr int32 MaxEntities = HktCore::MaxEntities;
    static constexpr int32 MaxProperties = HktCore::MaxProperties;

    /** SOA 레이아웃: Properties[PropertyId][EntityId] */
    TArray<TArray<int32>> Properties;

    /** 엔티티별 태그 컨테이너 */
    TArray<FGameplayTagContainer> EntityTags;

    /** 빈 태그 컨테이너 (Invalid Entity 반환용) */
    static const FGameplayTagContainer EmptyTagContainer;

    TBitArray<> ValidEntities;
    TArray<FHktEntityId> FreeList;
    int32 NextEntityId = 0;
    int32 CompletedFrameNumber = 0;

    /** 엔티티 생성 프레임 (Validation용) */
    TArray<int32> EntityCreationFrame;
};
