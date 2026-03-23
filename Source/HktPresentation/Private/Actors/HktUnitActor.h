// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HktSelectable.h"
#include "HktUnitActor.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;

/**
 * 캐릭터/유닛용 Actor.
 * IHktSelectable을 구현하여 커서 트레이스로 선택 가능.
 * 물리 충돌 없이 Visibility 채널만 응답 (QueryOnly).
 */
UCLASS(Blueprintable)
class AHktUnitActor : public AActor, public IHktSelectable
{
	GENERATED_BODY()

public:
	AHktUnitActor();

	/** EntityId 설정 (ActorRenderer에서 스폰 후 호출) */
	void SetEntityId(FHktEntityId InEntityId) { CachedEntityId = InEntityId; }

	// IHktSelectable
	virtual FHktEntityId GetEntityId() const override { return CachedEntityId; }

private:
	UPROPERTY(VisibleAnywhere, Category = "HKT|Unit")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, Category = "HKT|Unit")
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	FHktEntityId CachedEntityId = InvalidEntityId;
};
