// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktDatabaseTypes.h"
#include "HktPlayerDataProvider.h"
#include "Rules/HktServerRule.h"
#include "HktPlayerDatabaseComponent.generated.h"

/**
 * UHktPlayerDatabaseComponent - IHktWorldDatabase 구현
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - Actor(GameMode)는 이 컴포넌트를 Rule에 IHktWorldDatabase로 전달
 *
 * 역할:
 *   - IHktWorldDatabase: 비동기 플레이어 레코드 로드/저장
 *   - Provider 패턴으로 저장소 교체 가능 (파일 → Redis → SQL)
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktPlayerDatabaseComponent : public UActorComponent, public IHktWorldDatabase
{
    GENERATED_BODY()

public:
    UHktPlayerDatabaseComponent();

    // === IHktWorldDatabase 구현 ===

    virtual void LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(TUniquePtr<FHktPlayerRecord>)> InCallback) override;
    virtual void SavePlayerRecordAsync(FHktPlayerRecord InRecord) override;

    // === 기본값 설정 ===

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    int32 DefaultHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    int32 DefaultMaxHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    int32 DefaultAttackPower = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    int32 DefaultDefense = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    FGameplayTag DefaultVisualTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Database|Defaults")
    FGameplayTag DefaultFlowTag;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 저장소 Provider (파일/Redis/SQL) */
    TUniquePtr<IHktPlayerDataProvider> Provider;

    /** 캐시된 레코드 */
    TMap<int64, FHktPlayerRecord> CachedRecords;
};
