#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Engine/StreamableManager.h"
#include "HktAssetSubsystem.generated.h"

class UHktTagDataAsset;

/**
 * 태그 기반 에셋 관리 서브시스템입니다.
 * 게임 시작 시 HktTagDataAsset 타입의 에셋들을 스캔하여 태그 매핑 테이블을 구축합니다.
 */
UCLASS()
class HKTASSET_API UHktAssetSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static UHktAssetSubsystem* Get(UWorld* World);

    // 동기 로드: 이미 로드된 에셋을 찾거나, 없으면 동기로 로드하여 반환합니다.
    UHktTagDataAsset* LoadAssetSync(FGameplayTag Tag);

    // 비동기 로드 (기본형): 완료 신호만 받습니다. 람다 캡처를 통해 Tag를 가져와야 합니다.
    void LoadAssetAsync(FGameplayTag Tag, FStreamableDelegate DelegateToCall);

    // 비동기 로드 (편의형): 로드된 에셋을 람다 인자로 바로 전달받습니다.
    void LoadAssetAsync(FGameplayTag Tag, TFunction<void(UHktTagDataAsset*)> OnLoaded);

protected:
    void RebuildTagMap();

    // 로딩 완료 후 처리를 위한 내부 헬퍼
    void OnAssetLoadedInternal(FGameplayTag Tag, TFunction<void(UHktTagDataAsset*)> Callback);

private:
    // Tag와 SoftObjectPath를 매핑합니다.
    TMap<FGameplayTag, FSoftObjectPath> TagToPathMap;

    // 비동기 로딩 핸들 관리 (Garbage Collection 방지용으로 핸들을 잡아두고 싶다면 TSharedPtr<FStreamableHandle>을 저장해야 함)
    // 여기서는 단순 로드 요청용으로 사용합니다.
    FStreamableManager StreamableManager;
};