// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreInterfaces.h"

/**
 * FHktStashBase - Stash 공통 기능 구현
 * 
 * SOA 레이아웃으로 엔티티 데이터 저장
 * - Properties: 숫자 데이터 배열
 * - EntityTags: GameplayTagContainer 배열
 */
class FHktStashBase
{
public:
    FHktStashBase();
    virtual ~FHktStashBase() = default;

    // ========== IHktStashInterface 공통 구현 ==========
    FHktEntityId AllocateEntity();
    void FreeEntity(FHktEntityId Entity);
    bool IsValidEntity(FHktEntityId Entity) const;
    int32 GetProperty(FHktEntityId Entity, uint16 PropertyId) const;
    void SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value);
    int32 GetEntityCount() const;
    int32 GetCompletedFrameNumber() const { return CompletedFrameNumber; }
    void MarkFrameCompleted(int32 FrameNumber);
    void ForEachEntity(TFunctionRef<void(FHktEntityId)> Callback) const;
    uint32 CalculateChecksum() const;
    
    // ========== Tag API ==========
    const FGameplayTagContainer& GetTags(FHktEntityId Entity) const;
    void SetTags(FHktEntityId Entity, const FGameplayTagContainer& Tags);
    void AddTag(FHktEntityId Entity, const FGameplayTag& Tag);
    void RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag);
    bool HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const;
    bool HasTagExact(FHktEntityId Entity, const FGameplayTag& Tag) const;
    bool HasAnyTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const;
    bool HasAllTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const;
    FGameplayTag GetFirstTagWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const;
    FGameplayTagContainer GetTagsWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const;

protected:
    /** SetProperty 시 자동 엔티티 생성 여부 (VisibleStash에서 사용) */
    bool bAutoCreateOnSet = false;
    
    /** 변경 추적 (파생 클래스에서 오버라이드) */
    virtual void OnEntityDirty(FHktEntityId Entity) {}

    static constexpr int32 MaxEntities = 1024;
    static constexpr int32 MaxProperties = 128;  // 숫자 Property 수 축소 (태그로 대체)

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
};
