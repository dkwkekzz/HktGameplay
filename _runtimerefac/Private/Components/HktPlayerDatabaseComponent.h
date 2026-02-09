// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktDatabaseTypes.h"
#include "HktPlayerDataProvider.h"
#include "HktPlayerDatabaseComponent.generated.h"

/**
 * UHktPlayerDatabaseComponent - 플레이어 영구 데이터 관리 (서버 전용)
 * 
 * 책임:
 * - 플레이어 레코드 저장/로드
 * - 신규 플레이어 기본 엔티티 생성
 * - PersistentId ↔ RuntimeId 매핑 (서버에서만)
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktPlayerDatabaseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHktPlayerDatabaseComponent();

    // ========== 플레이어 레코드 ==========
    
    FHktPlayerRecord* GetPlayerRecord(const FString& PlayerId);
    const FHktPlayerRecord* GetPlayerRecord(const FString& PlayerId) const;
    bool HasPlayerRecord(const FString& PlayerId) const;
    /** 플레이어 레코드 로드/생성 후 콜백 호출. 캐시에 있으면 즉시, 없으면 Provider에서 로드. */
    void GetOrCreatePlayerRecord(const FString& PlayerId, TFunction<void(FHktPlayerRecord&)> Callback);
    /** 해당 플레이어만 저장소에 저장 */
    void SavePlayerRecord(const FHktPlayerRecord& Record);

    // ========== 기본 엔티티 생성 ==========
    
    /** 기본 캐릭터 생성 (Tag 기반) */
    UFUNCTION(BlueprintCallable, Category = "Hkt|Database")
    FHktEntityRecord CreateDefaultCharacterEntity();

    // ========== 런타임 매핑 (서버 전용) ==========
    
    void AddRuntimeMapping(const FString& PlayerId, FHktEntityId RuntimeId, const FGuid& PersistentId);
    FHktEntityId GetRuntimeId(const FString& PlayerId, const FGuid& PersistentId) const;
    FGuid GetPersistentId(const FString& PlayerId, FHktEntityId RuntimeId) const;
    void ClearPlayerMappings(const FString& PlayerId);
    
    /** 플레이어의 모든 RuntimeId 반환 */
    TArray<FHktEntityId> GetPlayerRuntimeIds(const FString& PlayerId) const;

    // ========== 기본값 설정 ==========
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    int32 DefaultHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    int32 DefaultMaxHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    int32 DefaultAttackPower = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    int32 DefaultDefense = 5;

    /** 기본 캐릭터 Visual 태그 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    FGameplayTag DefaultVisualTag;

    /** 기본 캐릭터 Flow 태그 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    FGameplayTag DefaultFlowTag;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TMap<FString, FHktPlayerRecord> PlayerRecords;
    TMap<FString, TArray<FHktRuntimeEntityMapping>> RuntimeMappings;
    TUniquePtr<IHktPlayerDataProvider> Provider;
};
