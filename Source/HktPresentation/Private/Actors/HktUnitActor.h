// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HktSelectable.h"
#include "HktUnitActor.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UHktAnimInstance;
struct FHktEntityPresentation;

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

	/** ViewModel 값을 Actor에 적용. bForceAll=true면 전체 초기화, false면 dirty만. */
	void ApplyPresentation(const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll);

	/** 매 프레임 Transform 적용 (Sync 주기와 렌더 주기 차이로 인한 끊김 방지) */
	void ApplyTransform(const FHktEntityPresentation& Entity);

private:
	UPROPERTY(VisibleAnywhere, Category = "HKT|Unit")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, Category = "HKT|Unit")
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	FHktEntityId CachedEntityId = InvalidEntityId;

	/** 캐시된 AnimInstance (매 프레임 FindComponent 방지) */
	TWeakObjectPtr<UHktAnimInstance> CachedAnimInstance;

	UHktAnimInstance* GetAnimInstance();
};
