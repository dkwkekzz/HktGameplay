// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktItemActor.h"
#include "Components/StaticMeshComponent.h"

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
