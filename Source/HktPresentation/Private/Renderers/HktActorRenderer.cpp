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
	CachedState = &State;
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

		// Transform: ViewModel의 RenderLocation/Rotation을 actor에 적용
		if (E->RenderLocation.IsDirty(Frame) || E->Rotation.IsDirty(Frame))
			ApplyTransform(Id, *E);

		UpdateAnimation(Id, *E, Frame);

		// 부착 상태 변경 (delta)
		if (E->OwnerEntity.IsDirty(Frame) || E->ItemState.IsDirty(Frame))
		{
			DetachFromOwner(Id);
			if (E->IsItemAttached())
				TryAttachToOwnerDirect(Id, static_cast<FHktEntityId>(E->OwnerEntity.Get()));
		}
	}

	// --- 보간 (전체 actor에 매 프레임 위치 적용) ---
	for (auto& [Id, WeakActor] : ActorMap)
	{
		if (AttachedItems.Contains(Id)) continue;
		if (!WeakActor.IsValid()) continue;
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E) continue;
		WeakActor->SetActorLocationAndRotation(
			E->RenderLocation.Get(), E->Rotation.Get(),
			false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void FHktActorRenderer::Teardown()
{
	AliveGuard.Reset();
	ActorMap.Empty();
	AttachedItems.Empty();
	CachedState = nullptr;
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
	FVector SpawnLocation = Entity.RenderLocation.Get();
	FRotator SpawnRotation = Entity.Rotation.Get();

	TWeakObjectPtr<ULocalPlayer> WeakLP = LocalPlayer;
	TWeakPtr<bool> WeakGuard = AliveGuard;
	AssetSubsystem->LoadAssetAsync(VisualTag, [WeakGuard, this, VisualTag, EntityId, SpawnLocation, SpawnRotation, WeakLP](UHktTagDataAsset* LoadedAsset)
	{
		if (!WeakGuard.IsValid()) return;

		ULocalPlayer* LP = WeakLP.Get();
		if (!LP) return;

		UWorld* CallbackWorld = LP->GetWorld();
		if (!CallbackWorld) return;

		AActor* SpawnedActor = nullptr;

		// --- 아이템 DataAsset 분기 ---
		if (UHktItemVisualDataAsset* ItemAsset = Cast<UHktItemVisualDataAsset>(LoadedAsset))
		{
			FActorSpawnParameters SpawnParams;
			AHktItemActor* ItemActor = CallbackWorld->SpawnActor<AHktItemActor>(AHktItemActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
			if (ItemActor)
			{
				ItemActor->SetupMesh(ItemAsset->Mesh, ItemAsset->MeshScale, ItemAsset->AttachRotationOffset, ItemAsset->AttachSocketName);
				ItemActor->SetEntityId(EntityId);
			}
			SpawnedActor = ItemActor;
		}
		else
		{
			// --- 캐릭터/NPC DataAsset 분기 ---
			TSubclassOf<AActor> ActorClass;
			UHktActorVisualDataAsset* VisualAsset = Cast<UHktActorVisualDataAsset>(LoadedAsset);
			if (VisualAsset && VisualAsset->ActorClass)
			{
				ActorClass = VisualAsset->ActorClass;
			}

			if (!ActorClass)
			{
				UE_LOG(LogHktPresentation, Warning, TEXT("SpawnActor: No ActorClass for tag %s"), *VisualTag.ToString());
				return;
			}

			FActorSpawnParameters SpawnParams;
			SpawnedActor = CallbackWorld->SpawnActor<AActor>(ActorClass, SpawnLocation, SpawnRotation, SpawnParams);
		}

		if (SpawnedActor)
		{
			if (ActorMap.Contains(EntityId))
			{
				SpawnedActor->Destroy();
				return;
			}

			HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation, FString::Printf(TEXT("SpawnActor Tag=%s Location=(%.1f, %.1f, %.1f)"), *VisualTag.ToString(), SpawnedActor->GetActorLocation().X, SpawnedActor->GetActorLocation().Y, SpawnedActor->GetActorLocation().Z), EntityId);

			ConfigureCollisionForSelection(SpawnedActor);

			if (AHktUnitActor* Unit = Cast<AHktUnitActor>(SpawnedActor))
			{
				Unit->SetEntityId(EntityId);
			}

			ActorMap.Add(EntityId, SpawnedActor);

			// ViewModel에서 직접 조회하여 즉시 초기화
			const FHktEntityPresentation* E = CachedState ? CachedState->Get(EntityId) : nullptr;
			if (E)
			{
				InitActorFromPresentation(SpawnedActor, EntityId, *E);

				if (E->IsItemAttached())
					TryAttachToOwnerDirect(EntityId, static_cast<FHktEntityId>(E->OwnerEntity.Get()));
			}

			AttachPendingItemsForOwner(EntityId);
		}
	});
}

void FHktActorRenderer::InitActorFromPresentation(AActor* Actor, FHktEntityId Id, const FHktEntityPresentation& Entity)
{
	// Transform 적용 (ViewModel의 RenderLocation 사용)
	Actor->SetActorLocationAndRotation(
		Entity.RenderLocation.Get(), Entity.Rotation.Get(),
		false, nullptr, ETeleportType::TeleportPhysics);

	// Animation 초기화
	UpdateAnimation(Id, Entity, 0, /*bForceUpdate=*/true);
}

void FHktActorRenderer::ApplyTransform(FHktEntityId Id, const FHktEntityPresentation& Entity)
{
	TWeakObjectPtr<AActor>* WeakPtr = ActorMap.Find(Id);
	if (!WeakPtr || !WeakPtr->IsValid()) return;
	if (AttachedItems.Contains(Id)) return;

	WeakPtr->Get()->SetActorLocationAndRotation(
		Entity.RenderLocation.Get(), Entity.Rotation.Get(),
		false, nullptr, ETeleportType::TeleportPhysics);
}

void FHktActorRenderer::DestroyActor(FHktEntityId Id)
{
	DetachFromOwner(Id);

	for (auto It = AttachedItems.CreateIterator(); It; ++It)
	{
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
}

void FHktActorRenderer::UpdateAnimation(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame, bool bForceUpdate)
{
	TWeakObjectPtr<AActor>* WeakPtr = ActorMap.Find(Id);
	if (!WeakPtr || !WeakPtr->IsValid()) return;

	AActor* Actor = WeakPtr->Get();
	USkeletalMeshComponent* SkelMesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh) return;

	UHktAnimInstance* HktAnim = Cast<UHktAnimInstance>(SkelMesh->GetAnimInstance());
	if (!HktAnim) return;

	if (bForceUpdate || Entity.bIsMoving.IsDirty(Frame))
		HktAnim->bIsMoving = Entity.bIsMoving.Get();

	if (bForceUpdate || Entity.Velocity.IsDirty(Frame))
	{
		FVector Vel = Entity.Velocity.Get();
		HktAnim->MoveSpeed = FVector2D(Vel.X, Vel.Y).Size();
		HktAnim->BlendSpaceX = HktAnim->MoveSpeed;
	}

	if (bForceUpdate || Entity.Stance.IsDirty(Frame))
		HktAnim->SyncStance(Entity.Stance.Get());

	if (bForceUpdate || Entity.AttackSpeed.IsDirty(Frame))
	{
		float SpeedScale = static_cast<float>(Entity.AttackSpeed.Get()) / 100.0f;
		if (SpeedScale <= 0.0f) SpeedScale = 1.0f;
		HktAnim->AttackPlayRate = SpeedScale;
	}

	if (bForceUpdate || Entity.CPRatio.IsDirty(Frame))
		HktAnim->CPRatio = Entity.CPRatio.Get();

	if (bForceUpdate || Entity.TagsDirtyFrame == Frame)
		HktAnim->SyncFromTagContainer(Entity.Tags);
}

void FHktActorRenderer::TryAttachToOwnerDirect(FHktEntityId ItemId, FHktEntityId OwnerId)
{
	AActor* ItemActor = GetActor(ItemId);
	AActor* OwnerActor = GetActor(OwnerId);
	if (!ItemActor || !OwnerActor) return;

	AHktItemActor* HktItem = Cast<AHktItemActor>(ItemActor);
	if (!HktItem || HktItem->GetAttachSocketName().IsNone()) return;

	USkeletalMeshComponent* SkelMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh) return;

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

void FHktActorRenderer::AttachPendingItemsForOwner(FHktEntityId OwnerEntityId)
{
	if (!CachedState) return;

	for (auto& [ExistingId, WeakActor] : ActorMap)
	{
		if (AttachedItems.Contains(ExistingId)) continue;
		const FHktEntityPresentation* E = CachedState->Get(ExistingId);
		if (E && E->IsItemAttached()
			&& static_cast<FHktEntityId>(E->OwnerEntity.Get()) == OwnerEntityId)
		{
			TryAttachToOwnerDirect(ExistingId, OwnerEntityId);
		}
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
