// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktItemActor.h"
#include "Components/StaticMeshComponent.h"

AHktItemActor::AHktItemActor()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	RootComponent = MeshComponent;

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHktItemActor::SetupMesh(UStaticMesh* InMesh, FVector Scale, FRotator AttachRotOffset)
{
	if (!MeshComponent) return;

	if (InMesh)
	{
		MeshComponent->SetStaticMesh(InMesh);
	}

	MeshComponent->SetRelativeScale3D(Scale);
	MeshComponent->SetRelativeRotation(AttachRotOffset);
}
