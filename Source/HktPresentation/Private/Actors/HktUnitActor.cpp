// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUnitActor.h"
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
