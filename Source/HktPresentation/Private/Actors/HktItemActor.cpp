// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktItemActor.h"
#include "HktPresentationState.h"
#include "HktPresentationLog.h"
#include "HktCoreEventLog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

AHktItemActor::AHktItemActor()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	RootComponent = MeshComponent;

	// QueryOnly: 커서 트레이스(Visibility 채널)에 응답, 물리 충돌은 없음
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AHktItemActor::SetupMesh(UStaticMesh* InMesh, FVector Scale, FRotator AttachRotOffset, FName InAttachSocketName)
{
	if (!MeshComponent) return;

	if (InMesh)
	{
		MeshComponent->SetStaticMesh(InMesh);
	}

	MeshComponent->SetRelativeScale3D(Scale);
	MeshComponent->SetRelativeRotation(AttachRotOffset);
	AttachSocketName = InAttachSocketName;
}

void AHktItemActor::ApplyPresentation(const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll,
	TFunctionRef<AActor*(FHktEntityId)> GetActorFunc)
{
	// --- 부착 상태 판단 ---
	if (bForceAll || Entity.OwnerEntity.IsDirty(Frame) || Entity.ItemState.IsDirty(Frame))
	{
		if (Entity.IsItemAttached())
		{
			if (!bIsAttachedToSocket)
				TryAttachToOwner(static_cast<FHktEntityId>(Entity.OwnerEntity.Get()), GetActorFunc);
		}
		else
		{
			DetachFromOwnerIfNeeded();
		}
	}

	// Transform은 ApplyTransform()에서 매 프레임 처리
}

void AHktItemActor::ApplyTransform(const FHktEntityPresentation& Entity)
{
	if (bIsAttachedToSocket) return;
	SetActorLocationAndRotation(
		Entity.RenderLocation.Get(), Entity.Rotation.Get(),
		false, nullptr, ETeleportType::TeleportPhysics);
}

void AHktItemActor::TryAttachToOwner(FHktEntityId OwnerId, TFunctionRef<AActor*(FHktEntityId)> GetActorFunc)
{
	AActor* OwnerActor = GetActorFunc(OwnerId);
	if (!OwnerActor) return;  // Owner 미스폰 → Owner 스폰 시 재시도됨

	if (AttachSocketName.IsNone()) return;

	USkeletalMeshComponent* SkelMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh) return;

	if (!SkelMesh->DoesSocketExist(AttachSocketName))
	{
		UE_LOG(LogHktPresentation, Warning, TEXT("Socket '%s' not found on owner %d for item %d"),
			*AttachSocketName.ToString(), OwnerId, CachedEntityId);
		return;
	}

	SetActorEnableCollision(false);
	AttachToComponent(SkelMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
	bIsAttachedToSocket = true;

	HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation,
		FString::Printf(TEXT("AttachItem Socket=%s Owner=%d"), *AttachSocketName.ToString(), OwnerId),
		CachedEntityId);
}

void AHktItemActor::DetachFromOwnerIfNeeded()
{
	if (!bIsAttachedToSocket) return;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorEnableCollision(true);
	bIsAttachedToSocket = false;

	HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation,
		FString::Printf(TEXT("DetachItem ItemId=%d"), CachedEntityId),
		CachedEntityId);
}
