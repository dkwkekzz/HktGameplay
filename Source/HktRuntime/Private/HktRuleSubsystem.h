#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HktRuleInterfaces.h"
#include "HktRuleSubsystem.generated.h"

/**
 * 규칙 관리 서브시스템입니다.
 */
UCLASS()
class HKTRUNTIME_API UHktRuleSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static UHktRuleSubsystem* Get(UWorld* World);

    void RegisterServerRule(TSharedPtr<IHktServerRule> InRule);
    void UnregisterServerRule();
    TSharedPtr<IHktServerRule> GetServerRule() const;

    void RegisterClientRule(TSharedPtr<IHktClientRule> InRule);
    void UnregisterClientRule();
    TSharedPtr<IHktClientRule> GetClientRule() const;

private:
    TSharedPtr<IHktServerRule> ServerRule;
    TSharedPtr<IHktClientRule> ClientRule;
};