#include "HktRuleSubsystem.h"

void UHktRuleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UHktRuleSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

UHktRuleSubsystem* UHktRuleSubsystem::Get(UWorld* World)
{
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UHktRuleSubsystem>();
    }
    return nullptr;
}

void UHktRuleSubsystem::RegisterServerRule(TSharedPtr<IHktServerRule> InRule)
{
    ServerRule = InRule;
}

void UHktRuleSubsystem::UnregisterServerRule()
{
    ServerRule.Reset();
}

TSharedPtr<IHktServerRule> UHktRuleSubsystem::GetServerRule() const
{
    return ServerRule;
}  

void UHktRuleSubsystem::RegisterClientRule(TSharedPtr<IHktClientRule> InRule)
{
    ClientRule = InRule;
}

void UHktRuleSubsystem::UnregisterClientRule()
{
    ClientRule.Reset();
}

TSharedPtr<IHktClientRule> UHktRuleSubsystem::GetClientRule() const
{
    return ClientRule;
}