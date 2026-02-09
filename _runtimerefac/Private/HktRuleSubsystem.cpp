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

void UHktRuleSubsystem::SetServerRule(TSharedPtr<IHktServerRule> InRule)
{
    ServerRule = InRule;
}

TSharedPtr<IHktServerRule> UHktRuleSubsystem::GetServerRule() const
{
    return ServerRule;
}  

void UHktRuleSubsystem::SetClientRule(TSharedPtr<IHktClientRule> InRule)
{
    ClientRule = InRule;
}

TSharedPtr<IHktClientRule> UHktRuleSubsystem::GetClientRule() const
{
    return ClientRule;
}