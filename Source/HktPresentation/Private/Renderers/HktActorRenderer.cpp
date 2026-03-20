// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktActorRenderer.h"
#include "HktAnimInstance.h"
#include "HktAssetSubsystem.h"
#include "DataAssets/HktActorVisualDataAsset.h"  // Public/DataAssets/
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/CapsuleComponent.h"
#include "HktCoreEventLog.h"

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

	// --- 비동기 스폰 완료 후 최초 동기화 ---
	for (auto It = PendingInitSync.CreateIterator(); It; ++It)
	{
		FHktEntityId Id = *It;
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E || !ActorMap.Contains(Id)) { It.RemoveCurrent(); continue; }
		UpdateMotionTarget(Id, *E, E->SpawnedFrame);
		UpdateAnimation(Id, *E, E->SpawnedFrame);
		if (E->IsItemAttached())
		{
			TryAttachToOwner(Id, State);
		}
		It.RemoveCurrent();
	}

	// --- 대기 중인 소켓 부착 재시도 (Owner Actor가 이번 프레임에 스폰되었을 수 있음) ---
	// TryAttachToOwner가 PendingAttachments를 수정하므로, 복사본으로 순회
	{
		TSet<FHktEntityId> PendingCopy = PendingAttachments;
		for (FHktEntityId ItemId : PendingCopy)
		{
			const FHktEntityPresentation* E = State.Get(ItemId);
			if (!E || !E->IsItemAttached())
			{
				PendingAttachments.Remove(ItemId);
				continue;
			}
			if (GetActor(ItemId) && GetActor(static_cast<FHktEntityId>(E->OwnerEntity.Get())))
			{
				TryAttachToOwner(ItemId, State);
			}
		}
	}

	// --- Dirty 엔티티 타겟 갱신 ---
	for (FHktEntityId Id : State.DirtyThisFrame)
	{
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E || E->RenderCategory != EHktRenderCategory::Actor) continue;
		if (!ActorMap.Contains(Id)) continue;
		UpdateMotionTarget(Id, *E, Frame);
		UpdateAnimation(Id, *E, Frame);

		// 소켓 부착 상태 변경 감지
		if (E->OwnerEntity.IsDirty(Frame) || E->ActionSlot.IsDirty(Frame))
		{
			// 기존 부착 해제 후 재부착 (ActionSlot 변경 시 소켓이 달라질 수 있으므로)
			DetachFromOwner(Id);
			if (E->IsItemAttached())
				TryAttachToOwner(Id, State);
		}
	}

	// --- 모든 활성 엔티티 보간 ---
	UWorld* World = LocalPlayer.IsValid() ? LocalPlayer->GetWorld() : nullptr;
	float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.016f;
	InterpolateActors(DeltaSeconds);
}

void FHktActorRenderer::Teardown()
{
	// 비동기 콜백 무효화 (this 접근 방지)
	AliveGuard.Reset();

	ActorMap.Empty();
	MotionStates.Empty();
	PendingInitSync.Empty();
	AttachedItems.Empty();
	PendingAttachments.Empty();
}

AActor* FHktActorRenderer::GetActor(FHktEntityId Id) const
{
	if (TWeakObjectPtr<AActor> const* P = ActorMap.Find(Id))
		return P->Get();
	return nullptr;
}

void FHktActorRenderer::SpawnActor(const FHktEntityPresentation& Entity)
{
	UWorld* World = LocalPlayer.IsValid() ? LocalPlayer->GetWorld() : nullptr;
	if (!World) return;

	UHktAssetSubsystem* AssetSubsystem = UHktAssetSubsystem::Get(World);
	if (!AssetSubsystem) return;

	FGameplayTag VisualTag = Entity.VisualElement.Get();
	if (!VisualTag.IsValid()) return;

	FVector Location = Entity.Location.Get();
	FRotator Rotation = Entity.Rotation.Get();
	FHktEntityId EntityId = Entity.EntityId;
	bool bIsMoving = Entity.bIsMoving.Get();

	// 스폰 시 지면 높이 적용
	float GroundZ;
	if (TraceGroundZ(World, Location, GroundZ))
	{
		Location.Z = GroundZ;
	}

	TWeakObjectPtr<ULocalPlayer> WeakLP = LocalPlayer;
	TWeakPtr<bool> WeakGuard = AliveGuard;
	AssetSubsystem->LoadAssetAsync(VisualTag, [WeakGuard, this, VisualTag, EntityId, Location, Rotation, bIsMoving, WeakLP](UHktTagDataAsset* LoadedAsset)
	{
		if (!WeakGuard.IsValid()) return;  // Renderer가 소멸됨

		ULocalPlayer* LP = WeakLP.Get();
		if (!LP) return;

		UWorld* CallbackWorld = LP->GetWorld();
		if (!CallbackWorld) return;

		TSubclassOf<AActor> ActorClass;

		// DataAsset 기반 ActorClass 해결
		UHktActorVisualDataAsset* VisualAsset = Cast<UHktActorVisualDataAsset>(LoadedAsset);
		if (VisualAsset && VisualAsset->ActorClass)
		{
			ActorClass = VisualAsset->ActorClass;
		}

		if (!ActorClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HktActorRenderer] SpawnActor: No ActorClass for tag %s (DataAsset not found or missing ActorClass)"), *VisualTag.ToString());
			return;
		}

		// 캡슐 반높이 오프셋 계산
		float HalfHeight = 0.0f;
		if (AActor* CDO = ActorClass->GetDefaultObject<AActor>())
		{
			if (UCapsuleComponent* Capsule = CDO->FindComponentByClass<UCapsuleComponent>())
			{
				HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			}
		}

		FVector SpawnLocation = Location;
		SpawnLocation.Z += HalfHeight;

		FActorSpawnParameters SpawnParams;
		AActor* SpawnedActor = CallbackWorld->SpawnActor<AActor>(ActorClass, SpawnLocation, Rotation, SpawnParams);
		if (SpawnedActor)
		{
			HKT_EVENT_LOG_ENTITY("Presentation", FString::Printf(TEXT("SpawnActor Tag=%s Location=(%.1f, %.1f, %.1f)"), *VisualTag.ToString(), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z), EntityId);

			SpawnedActor->SetActorEnableCollision(false);
			ActorMap.Add(EntityId, SpawnedActor);
			PendingInitSync.Add(EntityId);

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
	// 이 엔티티 자체가 부착된 아이템이면 해제
	DetachFromOwner(Id);

	// 이 엔티티를 Owner로 가지는 부착 아이템들 해제 (캐릭터 제거 시)
	for (auto It = AttachedItems.CreateIterator(); It; ++It)
	{
		// 아이템 Actor의 부모가 제거될 Actor이면 해제
		AActor* ItemActor = GetActor(*It);
		if (ItemActor && ItemActor->GetAttachParentActor() == GetActor(Id))
		{
			ItemActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			It.RemoveCurrent();
		}
	}

	if (TWeakObjectPtr<AActor>* P = ActorMap.Find(Id))
	{
		if (AActor* A = P->Get())
			A->Destroy();
		ActorMap.Remove(Id);
	}
	MotionStates.Remove(Id);
	PendingInitSync.Remove(Id);
}

void FHktActorRenderer::UpdateMotionTarget(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame)
{
	FHktActorMotionState& Motion = MotionStates.FindOrAdd(Id);

	if (Entity.Location.IsDirty(Frame))
	{
		FVector SimLocation = Entity.Location.Get();

		UWorld* World = LocalPlayer.IsValid() ? LocalPlayer->GetWorld() : nullptr;
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

	if (Entity.Rotation.IsDirty(Frame))
	{
		Motion.TargetRotation = Entity.Rotation.Get();
	}

	if (Entity.bIsMoving.IsDirty(Frame))
	{
		Motion.bIsMoving = Entity.bIsMoving.Get();
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
	if (Entity.bIsMoving.IsDirty(Frame))
	{
		HktAnim->bIsMoving = Entity.bIsMoving.Get();
	}

	// 속도 벡터에서 이동 속도 계산 — 블렌드스페이스 파라미터로 활용
	if (Entity.Velocity.IsDirty(Frame))
	{
		FVector Vel = Entity.Velocity.Get();
		HktAnim->MoveSpeed = FVector2D(Vel.X, Vel.Y).Size();
		HktAnim->BlendSpaceX = HktAnim->MoveSpeed;
	}

	// Stance 동기화 — Stance AnimBP 레이어 교체
	if (Entity.Stance.IsDirty(Frame))
	{
		HktAnim->SyncStance(Entity.Stance.Get());
	}

	// Entity TagContainer 기반 애니메이션 동기화
	// Story에서 AddTag/RemoveTag로 상태를 변경하면 AnimInstance가 태그 변화를 감지하여 애니메이션을 자동 재생
	if (Entity.TagsDirtyFrame == Frame)
	{
		HktAnim->SyncFromTagContainer(Entity.Tags);
	}
}

void FHktActorRenderer::InterpolateActors(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f) return;

	for (auto It = MotionStates.CreateIterator(); It; ++It)
	{
		FHktEntityId Id = It.Key();
		FHktActorMotionState& Motion = It.Value();

		// 소켓에 부착된 아이템은 소켓이 위치를 결정하므로 보간 건너뜀
		if (AttachedItems.Contains(Id))
			continue;

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

FName FHktActorRenderer::GetSocketName(int32 ActionSlot)
{
	switch (ActionSlot)
	{
	case 0:  return FName(TEXT("weapon_r"));
	case 1:  return FName(TEXT("shield_l"));
	default: return FName(*FString::Printf(TEXT("slot_%d"), ActionSlot));
	}
}

void FHktActorRenderer::TryAttachToOwner(FHktEntityId ItemId, const FHktPresentationState& State)
{
	const FHktEntityPresentation* ItemEntity = State.Get(ItemId);
	if (!ItemEntity || !ItemEntity->IsItemAttached()) return;

	FHktEntityId OwnerId = static_cast<FHktEntityId>(ItemEntity->OwnerEntity.Get());
	int32 Slot = ItemEntity->ActionSlot.Get();

	AActor* ItemActor = GetActor(ItemId);
	if (!ItemActor)
	{
		PendingAttachments.Add(ItemId);
		return;
	}

	AActor* OwnerActor = GetActor(OwnerId);
	if (!OwnerActor)
	{
		PendingAttachments.Add(ItemId);
		return;
	}

	USkeletalMeshComponent* SkelMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		PendingAttachments.Add(ItemId);
		return;
	}

	FName SocketName = GetSocketName(Slot);
	if (!SkelMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktActorRenderer] Socket '%s' not found on owner %d for item %d"), *SocketName.ToString(), OwnerId, ItemId);
		return;
	}

	ItemActor->AttachToComponent(SkelMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	AttachedItems.Add(ItemId);
	PendingAttachments.Remove(ItemId);

	HKT_EVENT_LOG_ENTITY("Presentation", FString::Printf(TEXT("AttachItem Slot=%d Socket=%s Owner=%d"), Slot, *SocketName.ToString(), OwnerId), ItemId);
}

void FHktActorRenderer::DetachFromOwner(FHktEntityId ItemId)
{
	if (!AttachedItems.Contains(ItemId)) return;

	AActor* ItemActor = GetActor(ItemId);
	if (ItemActor)
	{
		ItemActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	AttachedItems.Remove(ItemId);
	PendingAttachments.Remove(ItemId);

	HKT_EVENT_LOG_ENTITY("Presentation", TEXT("DetachItem"), ItemId);
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
