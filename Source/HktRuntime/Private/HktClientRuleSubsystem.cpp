#include "HktClientRuleSubsystem.h"

void UHktClientRuleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UHktClientRuleSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

UHktClientRuleSubsystem* UHktClientRuleSubsystem::Get(UWorld* World)
{
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UHktClientRuleSubsystem>();
    }
    return nullptr;
}
