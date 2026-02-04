// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Managers/HktEntityVisualManager.h"
#include "Settings/HktPresentationGlobalSetting.h"
#include "HktCoreInterfaces.h"
#include "HktPropertyIds.h"
#include "Actors/HktCharacter.h"
#include "Engine/World.h"

FHktEntityVisualManager::FHktEntityVisualManager(UWorld* InWorld)
	: World(InWorld)
{
}

FHktEntityVisualManager::~FHktEntityVisualManager()
{
	// 모든 스폰된 Character 정리
	for (auto& Pair : EntityCharacterMap)
	{
		if (AHktCharacter* Character = Pair.Value.Get())
		{
			Character->Destroy();
		}
	}
	EntityCharacterMap.Empty();
}

TSubclassOf<AHktCharacter> FHktEntityVisualManager::GetCharacterClass() const
{
	if (const UHktPresentationGlobalSetting* Settings = GetDefault<UHktPresentationGlobalSetting>())
	{
		return Settings->DefaultCharacterClass.LoadSynchronous();
	}
	return nullptr;
}

void FHktEntityVisualManager::OnEntityCreated(FHktEntityId EntityId, IHktStashInterface* Stash)
{
	TSubclassOf<AHktCharacter> CharacterClass = GetCharacterClass();
	if (!World || !CharacterClass || !Stash)
	{
		return;
	}

	// 이미 존재하면 스킵
	if (EntityCharacterMap.Contains(EntityId.RawValue))
	{
		return;
	}

	// 위치 획득
	FVector Position = GetPositionFromStash(EntityId, Stash);
	float Yaw = GetRotationFromStash(EntityId, Stash);

	// Character 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AHktCharacter* Character = World->SpawnActor<AHktCharacter>(
		CharacterClass,
		Position,
		FRotator(0.0f, Yaw, 0.0f),
		SpawnParams
	);

	if (Character)
	{
		Character->SetEntityId(EntityId);
		EntityCharacterMap.Add(EntityId.RawValue, Character);

		UE_LOG(LogTemp, Log, TEXT("[EntityVisualManager] Spawned Character for Entity %d at %s"),
			EntityId.RawValue, *Position.ToString());
	}
}

void FHktEntityVisualManager::OnEntityDestroyed(FHktEntityId EntityId)
{
	TWeakObjectPtr<AHktCharacter>* CharacterPtr = EntityCharacterMap.Find(EntityId.RawValue);
	if (!CharacterPtr)
	{
		return;
	}

	if (AHktCharacter* Character = CharacterPtr->Get())
	{
		Character->Destroy();
		UE_LOG(LogTemp, Log, TEXT("[EntityVisualManager] Destroyed Character for Entity %d"), EntityId.RawValue);
	}

	EntityCharacterMap.Remove(EntityId.RawValue);
}

void FHktEntityVisualManager::Tick(float DeltaTime, IHktStashInterface* Stash)
{
	if (!Stash)
	{
		return;
	}

	// 모든 엔티티의 위치/상태 동기화
	for (auto& Pair : EntityCharacterMap)
	{
		AHktCharacter* Character = Pair.Value.Get();
		if (!Character)
		{
			continue;
		}

		FHktEntityId EntityId(Pair.Key);

		if (!Stash->IsValidEntity(EntityId))
		{
			continue;
		}

		// 위치 동기화
		FVector NewPosition = GetPositionFromStash(EntityId, Stash);
		Character->SetActorLocation(NewPosition);

		// 회전 동기화
		float NewYaw = GetRotationFromStash(EntityId, Stash);
		Character->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
	}
}

AHktCharacter* FHktEntityVisualManager::GetCharacter(FHktEntityId EntityId) const
{
	if (const TWeakObjectPtr<AHktCharacter>* Found = EntityCharacterMap.Find(EntityId.RawValue))
	{
		return Found->Get();
	}
	return nullptr;
}

AActor* FHktEntityVisualManager::GetActor(FHktEntityId EntityId) const
{
	return GetCharacter(EntityId);
}

void FHktEntityVisualManager::ForEachCharacter(TFunctionRef<void(FHktEntityId, AHktCharacter*)> Callback) const
{
	for (const auto& Pair : EntityCharacterMap)
	{
		if (AHktCharacter* Character = Pair.Value.Get())
		{
			Callback(FHktEntityId(Pair.Key), Character);
		}
	}
}

TArray<FHktEntityId> FHktEntityVisualManager::GetAllEntityIds() const
{
	TArray<FHktEntityId> Result;
	Result.Reserve(EntityCharacterMap.Num());
	
	for (const auto& Pair : EntityCharacterMap)
	{
		Result.Add(FHktEntityId(Pair.Key));
	}
	
	return Result;
}

FVector FHktEntityVisualManager::GetPositionFromStash(FHktEntityId EntityId, IHktStashInterface* Stash) const
{
	int32 X = Stash->GetProperty(EntityId, PropertyId::PosX);
	int32 Y = Stash->GetProperty(EntityId, PropertyId::PosY);
	int32 Z = Stash->GetProperty(EntityId, PropertyId::PosZ);

	return FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
}

float FHktEntityVisualManager::GetRotationFromStash(FHktEntityId EntityId, IHktStashInterface* Stash) const
{
	int32 Yaw = Stash->GetProperty(EntityId, PropertyId::RotYaw);
	return static_cast<float>(Yaw);
}
