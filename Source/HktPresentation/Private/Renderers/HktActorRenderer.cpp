// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktActorRenderer.h"
#include "HktAnimInstance.h"
#include "HktRuntimeTags.h"
#include "HktAssetSubsystem.h"
#include "DataAssets/HktActorVisualDataAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/CapsuleComponent.h"

FHktActorRenderer::FHktActorRenderer(ULocalPlayer* InLP)
	: LocalPlayer(InLP)
{
}

void FHktActorRenderer::Sync(const FHktPresentationState& State)
{
	const int64 Frame = State.GetCurrentFrame();

	// --- 스폰 ---
	for (FHktEntityId Id : State.SpawnedThisFrame)
	{
		const FHktEntityPresentation* E = State.Get(Id);
		if (E && E->RenderCategory == EHktRenderCategory::Actor)
			SpawnActor(*E);
	}

	// --- 제거 ---
	for (FHktEntityId Id : State.RemovedThisFrame)
	{
		DestroyActor(Id);
	}

	// --- Dirty 엔티티 타겟 갱신 ---
	for (FHktEntityId Id : State.DirtyThisFrame)
	{
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E || E->RenderCategory != EHktRenderCategory::Actor) continue;
		if (!ActorMap.Contains(Id)) continue;
		UpdateMotionTarget(Id, *E, Frame);
		UpdateAnimation(Id, *E, Frame);
	}

	// --- 모든 활성 엔티티 보간 ---
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.016f;
	InterpolateActors(DeltaSeconds);
}

void FHktActorRenderer::Teardown()
{
	ActorMap.Empty();
	MotionStates.Empty();
}

AActor* FHktActorRenderer::GetActor(FHktEntityId Id) const
{
	if (TWeakObjectPtr<AActor> const* P = ActorMap.Find(Id))
		return P->Get();
	return nullptr;
}

void FHktActorRenderer::SpawnActor(const FHktEntityPresentation& Entity)
{
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!World) return;

	UHktAssetSubsystem* AssetSubsystem = UHktAssetSubsystem::Get(World);
	if (!AssetSubsystem) return;

	FGameplayTag VisualTag = Entity.Visualization.VisualElement.Get();
	if (!VisualTag.IsValid()) return;

	FVector Location = Entity.Transform.Location.Get();
	FRotator Rotation = Entity.Transform.Rotation.Get();
	FHktEntityId EntityId = Entity.EntityId;
	bool bIsMoving = Entity.Movement.bIsMoving.Get();

	// 스폰 시 지면 높이 적용
	float GroundZ;
	if (TraceGroundZ(World, Location, GroundZ))
	{
		Location.Z = GroundZ;
	}

	AssetSubsystem->LoadAssetAsync(VisualTag, [this, VisualTag, EntityId, Location, Rotation, bIsMoving, World](UHktTagDataAsset* LoadedAsset)
	{
		UHktActorVisualDataAsset* VisualAsset = Cast<UHktActorVisualDataAsset>(LoadedAsset);
		if (!VisualAsset || !VisualAsset->ActorClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HktActorRenderer] SpawnActor: No UHktActorVisualDataAsset or ActorClass for tag %s"), *VisualTag.ToString());
			return;
		}

		// 캡슐 반높이 오프셋 계산
		float HalfHeight = 0.0f;
		if (AActor* CDO = VisualAsset->ActorClass->GetDefaultObject<AActor>())
		{
			if (UCapsuleComponent* Capsule = CDO->FindComponentByClass<UCapsuleComponent>())
			{
				HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			}
		}

		FVector SpawnLocation = Location;
		SpawnLocation.Z += HalfHeight;

		FActorSpawnParameters SpawnParams;
		AActor* SpawnedActor = World->SpawnActor<AActor>(VisualAsset->ActorClass, SpawnLocation, Rotation, SpawnParams);
		if (SpawnedActor)
		{
			SpawnedActor->SetActorEnableCollision(false);
			ActorMap.Add(EntityId, SpawnedActor);

			// AnimInstance에 몽타주 매핑 주입
			if (USkeletalMeshComponent* SkelMesh = SpawnedActor->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (UHktAnimInstance* HktAnim = Cast<UHktAnimInstance>(SkelMesh->GetAnimInstance()))
				{
					HktAnim->InitMontageMappings(VisualAsset->MontageMappings);
					HktAnim->InitSequenceMappings(VisualAsset->SequenceMappings);
					HktAnim->InitBlendSpaceMappings(VisualAsset->BlendSpaceMappings);
				}
			}

			FHktActorMotionState& Motion = MotionStates.FindOrAdd(EntityId);
			Motion.TargetLocation = SpawnLocation;
			Motion.TargetRotation = Rotation;
			Motion.bIsMoving = bIsMoving;
			Motion.bNeedsGroundSnap = false;
		}
	});
}

void FHktActorRenderer::DestroyActor(FHktEntityId Id)
{
	if (TWeakObjectPtr<AActor>* P = ActorMap.Find(Id))
	{
		if (AActor* A = P->Get())
			A->Destroy();
		ActorMap.Remove(Id);
	}
	MotionStates.Remove(Id);
}

void FHktActorRenderer::UpdateMotionTarget(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame)
{
	FHktActorMotionState& Motion = MotionStates.FindOrAdd(Id);

	if (Entity.Transform.Location.IsDirty(Frame))
	{
		FVector SimLocation = Entity.Transform.Location.Get();

		UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
		float GroundZ = SimLocation.Z;
		if (World && TraceGroundZ(World, SimLocation, GroundZ))
		{
			SimLocation.Z = GroundZ;
		}

		// 캡슐 반높이 오프셋
		if (TWeakObjectPtr<AActor>* P = ActorMap.Find(Id))
		{
			if (AActor* Actor = P->Get())
			{
				if (UCapsuleComponent* Capsule = Actor->FindComponentByClass<UCapsuleComponent>())
				{
					SimLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
				}
			}
		}

		Motion.TargetLocation = SimLocation;
	}

	if (Entity.Transform.Rotation.IsDirty(Frame))
	{
		Motion.TargetRotation = Entity.Transform.Rotation.Get();
	}

	if (Entity.Movement.bIsMoving.IsDirty(Frame))
	{
		Motion.bIsMoving = Entity.Movement.bIsMoving.Get();
	}
}

void FHktActorRenderer::UpdateAnimation(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame)
{
	TWeakObjectPtr<AActor>* WeakPtr = ActorMap.Find(Id);
	if (!WeakPtr || !WeakPtr->IsValid())
	{
		return;
	}

	AActor* Actor = WeakPtr->Get();
	USkeletalMeshComponent* SkelMesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		return;
	}

	UHktAnimInstance* HktAnim = Cast<UHktAnimInstance>(SkelMesh->GetAnimInstance());
	if (!HktAnim)
	{
		return;
	}

	// 이동 상태 동기화
	if (Entity.Movement.bIsMoving.IsDirty(Frame))
	{
		HktAnim->bIsMoving = Entity.Movement.bIsMoving.Get();
	}

	// FullBody 루프 애니메이션 상태 변경 (Anim.Idle, Anim.Run 등)
	if (Entity.Animation.AnimState.IsDirty(Frame))
	{
		FGameplayTag AnimTag = Entity.Animation.AnimState.Get();
		HktAnim->SetAnimLayerTag(HktGameplayTags::Anim_Layer_FullBody, AnimTag);
	}

	// UpperBody 레이어 애니메이션 상태 변경
	if (Entity.Animation.AnimStateUpper.IsDirty(Frame))
	{
		FGameplayTag AnimTag = Entity.Animation.AnimStateUpper.Get();
		HktAnim->SetAnimLayerTag(HktGameplayTags::Anim_Layer_UpperBody, AnimTag);
	}

	// 원샷 몽타주 재생 (Anim.Montage.Attack 등)
	if (Entity.Animation.MontageState.IsDirty(Frame))
	{
		FGameplayTag MontageTag = Entity.Animation.MontageState.Get();
		if (MontageTag.IsValid())
		{
			HktAnim->PlayMontageByTag(MontageTag);
		}
	}
}

void FHktActorRenderer::InterpolateActors(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f) return;

	for (auto It = MotionStates.CreateIterator(); It; ++It)
	{
		FHktEntityId Id = It.Key();
		FHktActorMotionState& Motion = It.Value();

		TWeakObjectPtr<AActor>* WeakPtr = ActorMap.Find(Id);
		if (!WeakPtr || !WeakPtr->IsValid())
			continue;

		AActor* Actor = WeakPtr->Get();
		const FVector CurrentLocation = Actor->GetActorLocation();

		//// --- 위치: 단순 Lerp (매 프레임 50% → ~2틱에 도달) ---
		//FVector NewLocation;
		//if (FVector::DistSquared(CurrentLocation, Motion.TargetLocation) <= SnapDistance * SnapDistance)
		//{
		//	NewLocation = Motion.TargetLocation;
		//}
		//else
		//{
		//	NewLocation = FMath::Lerp(CurrentLocation, Motion.TargetLocation, LerpAlpha);
		//}
		//
		//// --- 회전: 이동 방향에서 직접 계산 ---
		//FRotator NewRotation = Motion.TargetRotation;
		//
		//Actor->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
		Actor->SetActorLocationAndRotation(Motion.TargetLocation, Motion.TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

bool FHktActorRenderer::TraceGroundZ(UWorld* World, const FVector& Pos, float& OutZ) const
{
	if (!World) return false;

	const FVector Start(Pos.X, Pos.Y, Pos.Z + TraceHalfHeight);
	const FVector End(Pos.X, Pos.Y, Pos.Z - TraceHalfHeight);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}

	return false;
}
