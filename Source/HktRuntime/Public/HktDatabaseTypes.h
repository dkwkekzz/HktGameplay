// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "HktDatabaseTypes.generated.h"

/**
 * FHktEntityRecord - 단일 엔티티의 영구 저장 데이터
 * 
 * Property: 숫자 데이터 (위치, 체력 등)
 * Tags: 모든 태그 (Visual, Flow, EntityType, Status 등)
 */
USTRUCT(BlueprintType)
struct HKTRUNTIME_API FHktEntityRecord
{
    GENERATED_BODY()

    /** 영구 고유 ID (DB 저장용) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    FGuid PersistentId;

    /** 숫자 Property 데이터 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    TArray<int32> Properties;

    /** 모든 태그 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    FGameplayTagContainer Tags;

    /** 미완료 이벤트들 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    TArray<FHktIntentEvent> PendingEvents;

    FHktEntityRecord()
    {
        PersistentId = FGuid::NewGuid();
    }

    // === Property 헬퍼 ===
    
    int32 GetProperty(uint16 PropId, int32 Default = 0) const
    {
        return Properties.IsValidIndex(PropId) ? Properties[PropId] : Default;
    }

    void SetProperty(uint16 PropId, int32 Value)
    {
        if (Properties.IsValidIndex(PropId) == false)
            Properties.SetNumZeroed(PropId - 1);
        Properties[PropId] = Value;
    }

    // === Tag 헬퍼 ===
    
    bool HasTag(const FGameplayTag& Tag) const { return Tags.HasTag(Tag); }
    void AddTag(const FGameplayTag& Tag) { Tags.AddTag(Tag); }
    void RemoveTag(const FGameplayTag& Tag) { Tags.RemoveTag(Tag); }
    
    /** 특정 부모 태그 아래의 첫 번째 태그 반환 */
    FGameplayTag GetFirstTagWithParent(const FGameplayTag& ParentTag) const
    {
        for (const FGameplayTag& Tag : Tags)
        {
            if (Tag.MatchesTag(ParentTag))
                return Tag;
        }
        return FGameplayTag();
    }

    bool IsValid() const { return PersistentId.IsValid(); }
};

/**
 * FHktPlayerRecord - 플레이어의 영구 저장 데이터
 */
USTRUCT(BlueprintType)
struct HKTRUNTIME_API FHktPlayerRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    FString PlayerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    TArray<FHktEntityRecord> OwnedEntities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 ActiveEntityIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    FDateTime LastLoginTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    FDateTime CreatedTime;

    FHktPlayerRecord()
    {
        CreatedTime = FDateTime::UtcNow();
        LastLoginTime = CreatedTime;
    }

    FHktEntityRecord* FindEntityByPersistentId(const FGuid& Id)
    {
        return OwnedEntities.FindByPredicate([&](const FHktEntityRecord& E)
        {
            return E.PersistentId == Id;
        });
    }

    FHktEntityRecord* GetActiveEntity()
    {
        if (OwnedEntities.Num() == 0) return nullptr;
        int32 Index = FMath::Clamp(ActiveEntityIndex, 0, OwnedEntities.Num() - 1);
        return &OwnedEntities[Index];
    }

    bool IsValid() const { return !PlayerId.IsEmpty(); }
    bool HasEntities() const { return OwnedEntities.Num() > 0; }
};

/**
 * FHktRuntimeEntityMapping - PersistentId ↔ RuntimeId 매핑 (서버 전용)
 */
USTRUCT()
struct HKTRUNTIME_API FHktRuntimeEntityMapping
{
    GENERATED_BODY()

    UPROPERTY()
    FHktEntityId RuntimeId = InvalidEntityId;

    UPROPERTY()
    FGuid PersistentId;

    bool IsValid() const { return RuntimeId != InvalidEntityId && PersistentId.IsValid(); }
};
