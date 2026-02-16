// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Components/ActorComponent.h"
#include "HktDatabaseTypes.h"
#include "HktServerRuleInterfaces.h"
#include "HktFileDatabaseComponent.generated.h"

/**
 * UHktFileDatabaseComponent - IHktWorldDatabase 구현 (파일 기반)
 *
 * Saved/HktPlayerDatabase/{PlayerId}.json 형태로 플레이어별 파일 저장.
 * 개발용/단일 서버 환경에서 사용.
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - Actor(GameMode)는 이 컴포넌트를 Rule에 IHktWorldDatabase로 전달
 *
 * 역할:
 *   - IHktWorldDatabase: 비동기 플레이어 레코드 로드/저장
 *   - 파일 I/O 직접 수행 (Provider 분리 없음)
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktFileDatabaseComponent : public UActorComponent, public IHktWorldDatabase
{
    GENERATED_BODY()

public:
    UHktFileDatabaseComponent();

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
    /** 파일 경로 / 로드 / 저장 */
    FString GetFilePath(const FString& PlayerId) const;
    static FString SanitizePlayerIdForPath(const FString& PlayerId);
    void LoadFromFile(const FString& PlayerId, TFunction<void(TOptional<FHktPlayerRecord>)> Callback);
    void SaveToFile(const FString& PlayerId, const FHktPlayerRecord& Record, TFunction<void(bool bSuccess)> Callback);

    /** 캐시된 레코드 */
    TMap<int64, FHktPlayerRecord> CachedRecords;
};
