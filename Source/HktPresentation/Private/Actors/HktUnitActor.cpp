// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUnitActor.h"
#include "HktAnimInstance.h"
#include "HktPresentationState.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

AHktUnitActor::AHktUnitActor()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	RootComponent = CapsuleComponent;

	// QueryOnly: 커서 트레이스(Visibility 채널)에 응답, 물리 충돌(밀어내기)은 없음
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CapsuleComponent->InitCapsuleSize(34.f, 88.f);

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(CapsuleComponent);
	MeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UHktAnimInstance* AHktUnitActor::GetAnimInstance()
{
	if (!CachedAnimInstance && MeshComponent)
	{
		CachedAnimInstance = Cast<UHktAnimInstance>(MeshComponent->GetAnimInstance());
	}
	return CachedAnimInstance;
}

void AHktUnitActor::ApplyPresentation(const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll)
{
	// --- Transform ---
	if (bForceAll || Entity.RenderLocation.IsDirty(Frame) || Entity.Rotation.IsDirty(Frame))
	{
		SetActorLocationAndRotation(
			Entity.RenderLocation.Get(), Entity.Rotation.Get(),
			false, nullptr, ETeleportType::TeleportPhysics);
	}

	// --- Animation ---
	UHktAnimInstance* HktAnim = GetAnimInstance();
	if (!HktAnim) return;

	if (bForceAll || Entity.bIsMoving.IsDirty(Frame))
		HktAnim->bIsMoving = Entity.bIsMoving.Get();

	if (bForceAll || Entity.Velocity.IsDirty(Frame))
	{
		FVector Vel = Entity.Velocity.Get();
		HktAnim->MoveSpeed = FVector2D(Vel.X, Vel.Y).Size();
		HktAnim->BlendSpaceX = HktAnim->MoveSpeed;
	}

	if (bForceAll || Entity.Stance.IsDirty(Frame))
		HktAnim->SyncStance(Entity.Stance.Get());

	if (bForceAll || Entity.AttackSpeed.IsDirty(Frame))
	{
		float SpeedScale = static_cast<float>(Entity.AttackSpeed.Get()) / 100.0f;
		if (SpeedScale <= 0.0f) SpeedScale = 1.0f;
		HktAnim->AttackPlayRate = SpeedScale;
	}

	if (bForceAll || Entity.CPRatio.IsDirty(Frame))
		HktAnim->CPRatio = Entity.CPRatio.Get();

	if (bForceAll || Entity.TagsDirtyFrame == Frame)
		HktAnim->SyncFromTagContainer(Entity.Tags);
}
