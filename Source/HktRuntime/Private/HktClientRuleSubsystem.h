#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HktClientRuleSubsystem.generated.h"

/**
 * 규칙 관리 서브시스템입니다.
 */
UCLASS()
class HKTRUNTIME_API UHktClientRuleSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static UHktClientRuleSubsystem* Get(UWorld* World);

};