// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktActorRenderer.h"
#include "HktPresentationLog.h"
#include "HktAssetSubsystem.h"
#include "DataAssets/HktActorVisualDataAsset.h"
#include "DataAssets/HktItemVisualDataAsset.h"
#include "Actors/HktItemActor.h"
#include "Actors/HktUnitActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HktCoreEventLog.h"

/** 모든 PrimitiveComponent를 QueryOnly + Visibility만 Block으로 설정 */
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

	// --- Dirty → Actor에 전달 (animation, attachment 등 delta 처리) ---
	for (FHktEntityId Id : State.DirtyThisFrame)
	{
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E || E->RenderCategory != EHktRenderCategory::Actor) continue;
		if (!ActorMap.Contains(Id)) continue;
		ForwardToActor(Id, *E, Frame, false);
	}

	// --- 매 프레임 Transform 적용 (Core와 렌더 주기 차이로 인한 끊김 방지) ---
	for (auto& [Id, WeakActor] : ActorMap)
	{
		if (!WeakActor.IsValid()) continue;
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E) continue;

		AActor* Actor = WeakActor.Get();
		if (AHktUnitActor* Unit = Cast<AHktUnitActor>(Actor))
			Unit->ApplyTransform(*E);
		else if (AHktItemActor* Item = Cast<AHktItemActor>(Actor))
			Item->ApplyTransform(*E);
	}
}

void FHktActorRenderer::ForwardToActor(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll)
{
	TWeakObjectPtr<AActor>* WeakPtr = ActorMap.Find(Id);
	if (!WeakPtr || !WeakPtr->IsValid()) return;

	AActor* Actor = WeakPtr->Get();

	if (AHktUnitActor* Unit = Cast<AHktUnitActor>(Actor))
	{
		Unit->ApplyPresentation(Entity, Frame, bForceAll);
	}
	else if (AHktItemActor* Item = Cast<AHktItemActor>(Actor))
	{
		Item->ApplyPresentation(Entity, Frame, bForceAll,
			[this](FHktEntityId OwnerId) -> AActor* { return GetActor(OwnerId); });
	}
}

void FHktActorRenderer::Teardown()
{
	AliveGuard.Reset();
	ActorMap.Empty();
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
			TSubclassOf<AActor> ActorClass;
			UHktActorVisualDataAsset* VisualAsset = Cast<UHktActorVisualDataAsset>(LoadedAsset);
			if (VisualAsset && VisualAsset->ActorClass)
				ActorClass = VisualAsset->ActorClass;

			if (!ActorClass)
			{
				HKT_EVENT_LOG(HktLogTags::Presentation, EHktLogLevel::Warning, EHktLogSource::Client,
					FString::Printf(TEXT("SpawnActor: No ActorClass for tag %s"), *VisualTag.ToString()));
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

			HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation, EHktLogLevel::Info, EHktLogSource::Client, FString::Printf(TEXT("SpawnActor Tag=%s Location=(%.1f, %.1f, %.1f)"), 
        *VisualTag.ToString(), SpawnedActor->GetActorLocation().X, SpawnedActor->GetActorLocation().Y, SpawnedActor->GetActorLocation().Z), EntityId);


			ConfigureCollisionForSelection(SpawnedActor);

			if (AHktUnitActor* Unit = Cast<AHktUnitActor>(SpawnedActor))
				Unit->SetEntityId(EntityId);

			ActorMap.Add(EntityId, SpawnedActor);

			// 최초 ViewModel 적용 (bForceAll = true)
			const FHktEntityPresentation* E = CachedState ? CachedState->Get(EntityId) : nullptr;
			if (E)
				ForwardToActor(EntityId, *E, 0, true);

			// Owner 스폰 시 → ViewModel 기반으로 대기 아이템 부착 시도
			if (CachedState)
			{
				for (auto& [ExistingId, WeakActor] : ActorMap)
				{
					if (ExistingId == EntityId) continue;
					if (!WeakActor.IsValid()) continue;
					const FHktEntityPresentation* ItemE = CachedState->Get(ExistingId);
					if (ItemE && ItemE->IsItemAttached()
						&& static_cast<FHktEntityId>(ItemE->OwnerEntity.Get()) == EntityId)
					{
						ForwardToActor(ExistingId, *ItemE, 0, true);
					}
				}
			}
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
}
