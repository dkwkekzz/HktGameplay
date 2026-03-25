// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Snippets/HktSnippetNPC.h"
#include "HktCoreProperties.h"
#include "HktStoryTags.h"
#include "HktRuntimeTags.h"

FHktStoryBuilder& HktSnippetNPC::SetupNPCStats(
	FHktStoryBuilder& B,
	const FGameplayTag& SpecificTag,
	const FHktNPCTemplate& Stats)
{
	using namespace Reg;
	using namespace HktStoryTags;
	using namespace HktGameplayTags;

	B.SaveConstEntity(Spawned, PropertyId::IsNPC, 1)
	 .SaveConstEntity(Spawned, PropertyId::Health, Stats.Health)
	 .SaveConstEntity(Spawned, PropertyId::MaxHealth, Stats.Health)
	 .SaveConstEntity(Spawned, PropertyId::AttackPower, Stats.AttackPower);

	if (Stats.Defense > 0)
	{
		B.SaveConstEntity(Spawned, PropertyId::Defense, Stats.Defense);
	}

	B.SaveConstEntity(Spawned, PropertyId::Team, Stats.Team)
	 .AddTag(Spawned, Entity_NPC)
	 .AddTag(Spawned, SpecificTag)
	 .AddTag(Spawned, Tag_NPC_Hostile);

	if (Stats.MaxSpeed > 0)
	{
		B.SaveConstEntity(Spawned, PropertyId::MaxSpeed, Stats.MaxSpeed);
	}

	return B;
}

FHktStoryBuilder& HktSnippetNPC::SpawnerLoopBegin(
	FHktStoryBuilder& B,
	const FString& LoopLabel,
	const FString& WaitLabel,
	const FGameplayTag& CountTag,
	int32 Cap)
{
	using namespace Reg;

	B.Label(LoopLabel)
	 .HasPlayerInGroup(Flag)
	 .JumpIfNot(Flag, WaitLabel)
	 .CountByTag(R0, CountTag)
	 .LoadConst(R1, Cap)
	 .CmpGe(Flag, R0, R1)
	 .JumpIf(Flag, WaitLabel);

	return B;
}

FHktStoryBuilder& HktSnippetNPC::SpawnerLoopEnd(
	FHktStoryBuilder& B,
	const FString& LoopLabel,
	const FString& WaitLabel,
	float IntervalSeconds)
{
	B.Label(WaitLabel)
	 .WaitSeconds(IntervalSeconds)
	 .Jump(LoopLabel);

	return B;
}

FHktStoryBuilder& HktSnippetNPC::SpawnNPCAtPosition(
	FHktStoryBuilder& B,
	const FGameplayTag& NPCTag,
	const FHktNPCTemplate& Stats,
	RegisterIndex PosBaseReg)
{
	using namespace Reg;

	B.Log(TEXT("[Snippet] SpawnNPCAtPosition"))
	 .SpawnEntity(NPCTag);
	SetupNPCStats(B, NPCTag, Stats);
	B.SetPosition(Spawned, PosBaseReg);

	return B;
}
