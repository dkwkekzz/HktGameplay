#include "HktServerRuleSubsystem.h"

void UHktServerRuleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UHktServerRuleSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

UHktServerRuleSubsystem* UHktServerRuleSubsystem::Get(UWorld* World)
{
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UHktServerRuleSubsystem>();
    }
    return nullptr;
}
