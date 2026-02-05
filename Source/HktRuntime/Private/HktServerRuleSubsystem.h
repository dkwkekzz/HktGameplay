#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HktServerRuleSubsystem.generated.h"

/**
 * 규칙 관리 서브시스템입니다.
 */
UCLASS()
class HKTRUNTIME_API UHktServerRuleSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static UHktServerRuleSubsystem* Get(UWorld* World);

    void OnReceived_RequestLogin(const FString& ID, const FString& PW, TFunction<void(bool bSuccess, const FString& Token)> ResultCallback);
};