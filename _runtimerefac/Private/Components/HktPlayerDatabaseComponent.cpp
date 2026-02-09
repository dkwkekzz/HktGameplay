// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPlayerDatabaseComponent.h"
#include "HktFilePlayerDataProvider.h"
#include "HktPropertyIds.h"

UHktPlayerDatabaseComponent::UHktPlayerDatabaseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    Provider = MakeUnique<FHktFilePlayerDataProvider>();

    // 기본 태그 설정
    DefaultVisualTag = FGameplayTag::RequestGameplayTag(TEXT("Visual.Character.Default"), false);
    DefaultFlowTag = FGameplayTag::RequestGameplayTag(TEXT("Flow.Character.Default"), false);
}

void UHktPlayerDatabaseComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UHktPlayerDatabaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (const auto& Pair : PlayerRecords)
    {
        Provider->Save(Pair.Key, Pair.Value, [](bool) {});
    }
    Super::EndPlay(EndPlayReason);
}

// ========== 플레이어 레코드 ==========

FHktPlayerRecord* UHktPlayerDatabaseComponent::GetPlayerRecord(const FString& PlayerId)
{
    return PlayerRecords.Find(PlayerId);
}

const FHktPlayerRecord* UHktPlayerDatabaseComponent::GetPlayerRecord(const FString& PlayerId) const
{
    return PlayerRecords.Find(PlayerId);
}

bool UHktPlayerDatabaseComponent::HasPlayerRecord(const FString& PlayerId) const
{
    return PlayerRecords.Contains(PlayerId);
}

void UHktPlayerDatabaseComponent::GetOrCreatePlayerRecord(const FString& PlayerId, TFunction<void(FHktPlayerRecord&)> Callback)
{
    if (FHktPlayerRecord* Existing = PlayerRecords.Find(PlayerId))
    {
        Existing->LastLoginTime = FDateTime::UtcNow();
        Callback(*Existing);
        return;
    }

    Provider->Load(PlayerId, [this, PlayerId, Callback](TOptional<FHktPlayerRecord> Loaded)
    {
        FHktPlayerRecord& Record = Loaded.IsSet()
            ? PlayerRecords.Add(PlayerId, MoveTemp(Loaded.GetValue()))
            : [this, PlayerId]() -> FHktPlayerRecord&
            {
                FHktPlayerRecord NewRecord;
                NewRecord.PlayerId = PlayerId;
                NewRecord.CreatedTime = FDateTime::UtcNow();
                NewRecord.LastLoginTime = NewRecord.CreatedTime;
                NewRecord.OwnedEntities.Add(CreateDefaultCharacterEntity());
                NewRecord.ActiveEntityIndex = 0;
                UE_LOG(LogTemp, Log, TEXT("[PlayerDatabase] Created new player: %s"), *PlayerId);
                return PlayerRecords.Add(PlayerId, MoveTemp(NewRecord));
            }();
        Record.LastLoginTime = FDateTime::UtcNow();
        Callback(Record);
    });
}

void UHktPlayerDatabaseComponent::SavePlayerRecord(const FHktPlayerRecord& Record)
{
    PlayerRecords.Add(Record.PlayerId, Record);
    Provider->Save(Record.PlayerId, Record, [](bool bSuccess)
    {
        if (!bSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("[PlayerDatabase] Save failed"));
        }
    });
}

// ========== 기본 엔티티 생성 ==========

FHktEntityRecord UHktPlayerDatabaseComponent::CreateDefaultCharacterEntity()
{
    FHktEntityRecord Entity;
    Entity.PersistentId = FGuid::NewGuid();
    
    // 태그 설정 (유연한 복수 태그)
    if (DefaultVisualTag.IsValid())
        Entity.Tags.AddTag(DefaultVisualTag);
    if (DefaultFlowTag.IsValid())
        Entity.Tags.AddTag(DefaultFlowTag);
    
    // EntityType 태그
    Entity.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("EntityType.Unit"), false));
    
    // 기본 스탯
    Entity.SetProperty(PropertyId::Health, DefaultHealth);
    Entity.SetProperty(PropertyId::MaxHealth, DefaultMaxHealth);
    Entity.SetProperty(PropertyId::AttackPower, DefaultAttackPower);
    Entity.SetProperty(PropertyId::Defense, DefaultDefense);
    
    UE_LOG(LogTemp, Log, TEXT("[PlayerDatabase] Created default character: %s"), 
        *Entity.PersistentId.ToString());
    
    return Entity;
}

// ========== 런타임 매핑 ==========

void UHktPlayerDatabaseComponent::AddRuntimeMapping(const FString& PlayerId, FHktEntityId RuntimeId, const FGuid& PersistentId)
{
    TArray<FHktRuntimeEntityMapping>& Mappings = RuntimeMappings.FindOrAdd(PlayerId);
    
    int32 Index = Mappings.IndexOfByPredicate([&](const FHktRuntimeEntityMapping& M)
    {
        return M.PersistentId == PersistentId;
    });
    
    if (Index != INDEX_NONE)
    {
        Mappings[Index].RuntimeId = RuntimeId;
    }
    else
    {
        FHktRuntimeEntityMapping Mapping;
        Mapping.RuntimeId = RuntimeId;
        Mapping.PersistentId = PersistentId;
        Mappings.Add(Mapping);
    }
}

FHktEntityId UHktPlayerDatabaseComponent::GetRuntimeId(const FString& PlayerId, const FGuid& PersistentId) const
{
    const TArray<FHktRuntimeEntityMapping>* Mappings = RuntimeMappings.Find(PlayerId);
    if (!Mappings) return InvalidEntityId;
    
    const FHktRuntimeEntityMapping* Found = Mappings->FindByPredicate([&](const FHktRuntimeEntityMapping& M)
    {
        return M.PersistentId == PersistentId;
    });
    
    return Found ? Found->RuntimeId : InvalidEntityId;
}

FGuid UHktPlayerDatabaseComponent::GetPersistentId(const FString& PlayerId, FHktEntityId RuntimeId) const
{
    const TArray<FHktRuntimeEntityMapping>* Mappings = RuntimeMappings.Find(PlayerId);
    if (!Mappings) return FGuid();
    
    const FHktRuntimeEntityMapping* Found = Mappings->FindByPredicate([&](const FHktRuntimeEntityMapping& M)
    {
        return M.RuntimeId == RuntimeId;
    });
    
    return Found ? Found->PersistentId : FGuid();
}

void UHktPlayerDatabaseComponent::ClearPlayerMappings(const FString& PlayerId)
{
    RuntimeMappings.Remove(PlayerId);
}

TArray<FHktEntityId> UHktPlayerDatabaseComponent::GetPlayerRuntimeIds(const FString& PlayerId) const
{
    TArray<FHktEntityId> Result;
    const TArray<FHktRuntimeEntityMapping>* Mappings = RuntimeMappings.Find(PlayerId);
    if (Mappings)
    {
        for (const FHktRuntimeEntityMapping& M : *Mappings)
        {
            Result.Add(M.RuntimeId);
        }
    }
    return Result;
}

