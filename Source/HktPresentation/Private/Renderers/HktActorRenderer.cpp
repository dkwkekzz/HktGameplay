// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktActorRenderer.h"
#include "HktPresentationLog.h"
#include "HktAnimInstance.h"
#include "HktAssetSubsystem.h"
#include "DataAssets/HktActorVisualDataAsset.h"
#include "DataAssets/HktItemVisualDataAsset.h"
#include "Actors/HktItemActor.h"
#include "Actors/HktUnitActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/CapsuleComponent.h"
#include "HktCoreEventLog.h"

/** 모든 PrimitiveComponent를 QueryOnly + Visibility만 Block으로 설정 (밀어내기 없이 커서 선택만 가능) */
static void ConfigureCollisionForSelection(AActor* Actor)
{
	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	Actor->GetComponents(Primitives);
	for (UPrimitiveComponent* Prim : Primitives)
	{
		Prim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Prim->SetCollisionResponseToAllChannels(ECR_Ignore);
		Prim->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

FHktActorRenderer::FHktActorRenderer(ULocalPlayer* InLP)
	: LocalPlayer(InLP)
{
}

void FHktActorRenderer::Sync(const FHktPresentationState& State)
{
	const int64 Frame = State.GetCurrentFrame();

	// --- 스폰 → async load 트리거 ---
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

	// --- Dirty 엔티티 delta 처리 ---
	for (FHktEntityId Id : State.DirtyThisFrame)
	{
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E || E->RenderCategory != EHktRenderCategory::Actor) continue;
		if (!ActorMap.Contains(Id)) continue;
		UpdateMotionTarget(Id, *E, Frame);
		UpdateAnimation(Id, *E, Frame);

		// 부착 상태 변경 (delta)
		if (E->OwnerEntity.IsDirty(Frame) || E->ItemState.IsDirty(Frame))
		{
			DetachFromOwner(Id);
			if (E->IsItemAttached())
				TryAttachToOwnerDirect(Id, static_cast<FHktEntityId>(E->OwnerEntity.Get()));
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
	AttachedItems.Empty();
	PendingItemsByOwner.Empty();
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

	FHktEntityId EntityId = Entity.EntityId;

	// ViewModel 스냅샷 캡처 (async 콜백에서 초기화에 사용)
	FHktSpawnSnapshot Snap;
	Snap.Location = Entity.Location.Get();
	Snap.Rotation = Entity.Rotation.Get();
	Snap.bIsMoving = Entity.bIsMoving.Get();
	Snap.Velocity = Entity.Velocity.Get();
	Snap.Stance = Entity.Stance.Get();
	Snap.Tags = Entity.Tags;
	Snap.AttackSpeed = static_cast<float>(Entity.AttackSpeed.Get());
	Snap.CPRatio = Entity.CPRatio.Get();
	Snap.OwnerEntity = Entity.OwnerEntity.Get();
	Snap.ItemState = Entity.ItemState.Get();

	// 스폰 시 지면 높이 적용
	float GroundZ;
	if (TraceGroundZ(World, Snap.Location, GroundZ))
	{
		Snap.Location.Z = GroundZ;
	}

	TWeakObjectPtr<ULocalPlayer> WeakLP = LocalPlayer;
	TWeakPtr<bool> WeakGuard = AliveGuard;
	AssetSubsystem->LoadAssetAsync(VisualTag, [WeakGuard, this, VisualTag, EntityId, Snap, WeakLP](UHktTagDataAsset* LoadedAsset)
	{
		if (!WeakGuard.IsValid()) return;  // Renderer가 소멸됨

		ULocalPlayer* LP = WeakLP.Get();
		if (!LP) return;

		UWorld* CallbackWorld = LP->GetWorld();
		if (!CallbackWorld) return;

		AActor* SpawnedActor = nullptr;

		// --- 아이템 DataAsset 분기: 메시 기반 데이터 드리븐 스폰 ---
		if (UHktItemVisualDataAsset* ItemAsset = Cast<UHktItemVisualDataAsset>(LoadedAsset))
		{
			FActorSpawnParameters SpawnParams;
			AHktItemActor* ItemActor = CallbackWorld->SpawnActor<AHktItemActor>(AHktItemActor::StaticClass(), Snap.Location, Snap.Rotation, SpawnParams);
			if (ItemActor)
			{
				ItemActor->SetupMesh(ItemAsset->Mesh, ItemAsset->MeshScale, ItemAsset->AttachRotationOffset, ItemAsset->AttachSocketName);
				ItemActor->SetEntityId(EntityId);
			}
			SpawnedActor = ItemActor;
		}
		else
		{
			// --- 캐릭터/NPC DataAsset 분기: Blueprint 클래스 스폰 ---
			TSubclassOf<AActor> ActorClass;
			UHktActorVisualDataAsset* VisualAsset = Cast<UHktActorVisualDataAsset>(LoadedAsset);
			if (VisualAsset && VisualAsset->ActorClass)
			{
				ActorClass = VisualAsset->ActorClass;
			}

			if (!ActorClass)
			{
				UE_LOG(LogHktPresentation, Warning, TEXT("SpawnActor: No ActorClass for tag %s (DataAsset not found or missing ActorClass)"), *VisualTag.ToString());
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

			FVector SpawnLocation = Snap.Location;
			SpawnLocation.Z += HalfHeight;

			FActorSpawnParameters SpawnParams;
			SpawnedActor = CallbackWorld->SpawnActor<AActor>(ActorClass, SpawnLocation, Snap.Rotation, SpawnParams);
		}

		if (SpawnedActor)
		{
			// 비동기 로드 중 엔티티가 제거되었거나 재사용된 경우 → 즉시 파괴
			if (ActorMap.Contains(EntityId))
			{
				SpawnedActor->Destroy();
				return;
			}

			HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation, FString::Printf(TEXT("SpawnActor Tag=%s Location=(%.1f, %.1f, %.1f)"), *VisualTag.ToString(), SpawnedActor->GetActorLocation().X, SpawnedActor->GetActorLocation().Y, SpawnedActor->GetActorLocation().Z), EntityId);

			ConfigureCollisionForSelection(SpawnedActor);

			// 캐릭터 Actor에 EntityId 설정 (IHktSelectable 커서 선택용)
			if (AHktUnitActor* Unit = Cast<AHktUnitActor>(SpawnedActor))
			{
				Unit->SetEntityId(EntityId);
			}

			ActorMap.Add(EntityId, SpawnedActor);

			// 즉시 초기화 — PendingInitSync 불필요
			InitActorFromSnapshot(SpawnedActor, EntityId, Snap);

			// 아이템 부착 처리
			if (Snap.OwnerEntity != InvalidEntityId && Snap.ItemState == 2)
			{
				TryAttachToOwnerDirect(EntityId, static_cast<FHktEntityId>(Snap.OwnerEntity));
			}

			// 캐릭터/NPC라면 → 대기 중인 아이템들 부착
			ProcessPendingAttachmentsForOwner(EntityId);
		}
	});
}

void FHktActorRenderer::InitActorFromSnapshot(AActor* Actor, FHktEntityId Id, const FHktSpawnSnapshot& Snap)
{
	// MotionState 초기화
	FHktActorMotionState& Motion = MotionStates.FindOrAdd(Id);
	Motion.TargetLocation = Actor->GetActorLocation();
	Motion.TargetRotation = Snap.Rotation;
	Motion.bIsMoving = Snap.bIsMoving;
	Motion.bNeedsGroundSnap = false;

	// Animation 초기화
	USkeletalMeshComponent* SkelMesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh) return;

	UHktAnimInstance* HktAnim = Cast<UHktAnimInstance>(SkelMesh->GetAnimInstance());
	if (!HktAnim) return;

	HktAnim->bIsMoving = Snap.bIsMoving;
	HktAnim->MoveSpeed = FVector2D(Snap.Velocity.X, Snap.Velocity.Y).Size();
	HktAnim->BlendSpaceX = HktAnim->MoveSpeed;
	HktAnim->SyncStance(Snap.Stance);

	float SpeedScale = Snap.AttackSpeed / 100.0f;
	if (SpeedScale <= 0.0f) SpeedScale = 1.0f;
	HktAnim->AttackPlayRate = SpeedScale;

	HktAnim->CPRatio = Snap.CPRatio;
	HktAnim->SyncFromTagContainer(Snap.Tags);
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

	// 이 Owner를 기다리는 대기 아이템들 정리
	PendingItemsByOwner.Remove(Id);

	if (TWeakObjectPtr<AActor>* P = ActorMap.Find(Id))
	{
		if (AActor* A = P->Get())
			A->Destroy();
		ActorMap.Remove(Id);
	}
	MotionStates.Remove(Id);
}

void FHktActorRenderer::UpdateMotionTarget(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame, bool bForceUpdate)
{
	FHktActorMotionState& Motion = MotionStates.FindOrAdd(Id);

	if (bForceUpdate || Entity.Location.IsDirty(Frame))
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

	if (bForceUpdate || Entity.Rotation.IsDirty(Frame))
	{
		Motion.TargetRotation = Entity.Rotation.Get();
	}

	if (bForceUpdate || Entity.bIsMoving.IsDirty(Frame))
	{
		Motion.bIsMoving = Entity.bIsMoving.Get();
	}
}

void FHktActorRenderer::UpdateAnimation(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame, bool bForceUpdate)
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
	if (bForceUpdate || Entity.bIsMoving.IsDirty(Frame))
	{
		HktAnim->bIsMoving = Entity.bIsMoving.Get();
	}

	// 속도 벡터에서 이동 속도 계산 — 블렌드스페이스 파라미터로 활용
	if (bForceUpdate || Entity.Velocity.IsDirty(Frame))
	{
		FVector Vel = Entity.Velocity.Get();
		HktAnim->MoveSpeed = FVector2D(Vel.X, Vel.Y).Size();
		HktAnim->BlendSpaceX = HktAnim->MoveSpeed;
	}

	// Stance 동기화 — Stance AnimBP 레이어 교체
	if (bForceUpdate || Entity.Stance.IsDirty(Frame))
	{
		HktAnim->SyncStance(Entity.Stance.Get());
	}

	// AttackSpeed → 몽타주 PlayRate 동기화
	if (bForceUpdate || Entity.AttackSpeed.IsDirty(Frame))
	{
		float SpeedScale = static_cast<float>(Entity.AttackSpeed.Get()) / 100.0f;
		if (SpeedScale <= 0.0f) SpeedScale = 1.0f;
		HktAnim->AttackPlayRate = SpeedScale;
	}

	// CP 비율 동기화 (UI 피드백용)
	if (bForceUpdate || Entity.CPRatio.IsDirty(Frame))
	{
		HktAnim->CPRatio = Entity.CPRatio.Get();
	}

	// Entity TagContainer 기반 애니메이션 동기화
	if (bForceUpdate || Entity.TagsDirtyFrame == Frame)
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
		Actor->SetActorLocationAndRotation(Motion.TargetLocation, Motion.TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void FHktActorRenderer::TryAttachToOwnerDirect(FHktEntityId ItemId, FHktEntityId OwnerId)
{
	AActor* ItemActor = GetActor(ItemId);
	AActor* OwnerActor = GetActor(OwnerId);

	if (!ItemActor || !OwnerActor)
	{
		// Owner가 아직 없으면 대기 맵에 등록
		PendingItemsByOwner.Add(OwnerId, ItemId);
		return;
	}

	// 아이템 Actor에서 DataAsset이 지정한 소켓 이름을 가져옴
	AHktItemActor* HktItem = Cast<AHktItemActor>(ItemActor);
	if (!HktItem || HktItem->GetAttachSocketName().IsNone())
	{
		return;
	}

	USkeletalMeshComponent* SkelMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		PendingItemsByOwner.Add(OwnerId, ItemId);
		return;
	}

	FName SocketName = HktItem->GetAttachSocketName();
	if (!SkelMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(LogHktPresentation, Warning, TEXT("Socket '%s' not found on owner %d for item %d"), *SocketName.ToString(), OwnerId, ItemId);
		return;
	}

	ItemActor->SetActorEnableCollision(false);
	ItemActor->AttachToComponent(SkelMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	AttachedItems.Add(ItemId);

	HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation, FString::Printf(TEXT("AttachItem Socket=%s Owner=%d"), *SocketName.ToString(), OwnerId), ItemId);
}

void FHktActorRenderer::ProcessPendingAttachmentsForOwner(FHktEntityId OwnerEntityId)
{
	AActor* OwnerActor = GetActor(OwnerEntityId);
	if (!OwnerActor) return;

	TArray<FHktEntityId> ItemIds;
	PendingItemsByOwner.MultiFind(OwnerEntityId, ItemIds);
	PendingItemsByOwner.Remove(OwnerEntityId);

	for (FHktEntityId ItemId : ItemIds)
	{
		TryAttachToOwnerDirect(ItemId, OwnerEntityId);
	}
}

void FHktActorRenderer::DetachFromOwner(FHktEntityId ItemId)
{
	if (!AttachedItems.Contains(ItemId)) return;

	AActor* ItemActor = GetActor(ItemId);
	if (ItemActor)
	{
		ItemActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		ConfigureCollisionForSelection(ItemActor);
	}

	AttachedItems.Remove(ItemId);

	HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation,
		FString::Printf(TEXT("DetachItem ItemId=%d"), ItemId), ItemId);
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
