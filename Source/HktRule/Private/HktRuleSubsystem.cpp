// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktRuleSubsystem.h"
#include "HktServerRule.h"
#include "HktClientRule.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void UHktRuleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ServerRule = MakeUnique<FHktDefaultServerRule>();
	ClientRule = MakeUnique<FHktDefaultClientRule>();
}

void UHktRuleSubsystem::Deinitialize()
{
	ServerRule.Reset();
	ClientRule.Reset();

	Super::Deinitialize();
}

UHktRuleSubsystem* UHktRuleSubsystem::Get(UWorld* World)
{
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UHktRuleSubsystem>() : nullptr;
}

TUniquePtr<IHktClientRule> UHktRuleSubsystem::CreateClientRule()
{
	return MakeUnique<FHktDefaultClientRule>();
}

IHktServerRule* HktRule::GetServerRule(UWorld* World)
{
	if (!World) return nullptr;
	UHktRuleSubsystem* Subsystem = UHktRuleSubsystem::Get(World);
	return Subsystem ? Subsystem->GetServerRule() : nullptr;
}

IHktClientRule* HktRule::GetClientRule(UWorld* World)
{
	if (!World) return nullptr;
	UHktRuleSubsystem* Subsystem = UHktRuleSubsystem::Get(World);
	return Subsystem ? Subsystem->GetClientRule() : nullptr;
}