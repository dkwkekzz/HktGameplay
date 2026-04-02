// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktItemActor.h"
#include "HktPresentationState.h"
#include "HktPresentationLog.h"
#include "HktCoreEventLog.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

AHktItemActor::AHktItemActor()
{
	// 메시보다 큰 투명 구체 콜리전 → 커서 클릭 판정 확대
	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->InitSphereRadius(80.f);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickupCollision->SetGenerateOverlapEvents(false);
	PickupCollision->ShapeColor = FColor(0, 255, 0, 64);
	PickupCollision->SetHiddenInGame(true);
	RootComponent = PickupCollision;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	MeshComponent->SetupAttachment(PickupCollision);

	// 메시 자체는 충돌 불필요 (PickupCollision이 담당)
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	// --- 부착/소유 상태 판단 ---
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

			// 비장착 소유 아이템(InBag 등)은 월드에서 숨김, Ground면 표시
			// 장착 가능 아이템은 소켓 부착으로 처리되므로 여기서 숨기지 않음
			const bool bShouldHide = Entity.IsItemOwned();
			SetActorHiddenInGame(bShouldHide);
			SetActorEnableCollision(!bShouldHide);
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
		HKT_EVENT_LOG(HktLogTags::Presentation, EHktLogLevel::Warning, EHktLogSource::Client,
			FString::Printf(TEXT("Socket '%s' not found on owner %d for item %d"),
			*AttachSocketName.ToString(), OwnerId, CachedEntityId));
		return;
	}

	SetActorEnableCollision(false);
	AttachToComponent(SkelMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
	bIsAttachedToSocket = true;

	HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation, EHktLogLevel::Info, EHktLogSource::Client,
		FString::Printf(TEXT("AttachItem Socket=%s Owner=%d"), *AttachSocketName.ToString(), OwnerId),
		CachedEntityId);
}

void AHktItemActor::DetachFromOwnerIfNeeded()
{
	if (!bIsAttachedToSocket) return;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorEnableCollision(true);
	bIsAttachedToSocket = false;

	HKT_EVENT_LOG_ENTITY(HktLogTags::Presentation, EHktLogLevel::Info, EHktLogSource::Client,
		FString::Printf(TEXT("DetachItem ItemId=%d"), CachedEntityId),
		CachedEntityId);
}
