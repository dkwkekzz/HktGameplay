// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktPresentationTypes.h"

class UWorld;
class AHktCharacter;
class IHktStashInterface;

/**
 * FHktEntityVisualManager
 * 
 * 엔티티 시각화 관리
 * - Stash의 엔티티 생성/파괴에 따라 AHktCharacter Spawn/Destroy
 * - 엔티티 정보를 Character에 주입
 * - 매 틱 위치/상태 동기화
 */
class HKTPRESENTATION_API FHktEntityVisualManager
{
public:
	FHktEntityVisualManager(UWorld* InWorld);
	~FHktEntityVisualManager();

	// === 엔티티 관리 ===
	
	/** 엔티티 생성 처리 - Character 스폰 */
	void OnEntityCreated(FHktEntityId EntityId, IHktStashInterface* Stash);
	
	/** 엔티티 파괴 처리 - Character 파괴 */
	void OnEntityDestroyed(FHktEntityId EntityId);

	/** 매 틱 엔티티 위치/상태 동기화 */
	void Tick(float DeltaTime, IHktStashInterface* Stash);

	// === 조회 ===
	
	/** EntityId로 Character 획득 */
	AHktCharacter* GetCharacter(FHktEntityId EntityId) const;

	/** EntityId로 Actor 획득 (Character의 부모 타입) */
	AActor* GetActor(FHktEntityId EntityId) const;

	/** 모든 관리 중인 Character 순회 */
	void ForEachCharacter(TFunctionRef<void(FHktEntityId, AHktCharacter*)> Callback) const;

	/** 모든 EntityId 획득 */
	TArray<FHktEntityId> GetAllEntityIds() const;

	/** 엔티티 개수 */
	int32 GetEntityCount() const { return EntityCharacterMap.Num(); }

private:
	FVector GetPositionFromStash(FHktEntityId EntityId, IHktStashInterface* Stash) const;
	float GetRotationFromStash(FHktEntityId EntityId, IHktStashInterface* Stash) const;

private:
	TSubclassOf<AHktCharacter> GetCharacterClass() const;

private:
	UWorld* World = nullptr;
	
	// EntityId → Character 매핑
	TMap<int32, TWeakObjectPtr<AHktCharacter>> EntityCharacterMap;
};
