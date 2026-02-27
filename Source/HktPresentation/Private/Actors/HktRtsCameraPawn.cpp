// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Actors/HktRtsCameraPawn.h"
#include "HktPresentationSubsystem.h"
#include "HktPresentationState.h"
#include "IHktPlayerInteractionInterface.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

AHktRtsCameraPawn::AHktRtsCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->bDoCollisionTest = false;
	SpringArm->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	SpringArm->TargetArmLength = 2000.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
}

void AHktRtsCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetController<APlayerController>();
	BoundPlayerController = PC;

	IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(PC);
	if (Interaction)
	{
		WheelInputHandle = Interaction->OnWheelInput().AddUObject(this, &AHktRtsCameraPawn::HandleZoom);
		SubjectChangedHandle = Interaction->OnSubjectChanged().AddUObject(this, &AHktRtsCameraPawn::OnSubjectChanged);
	}
}

void AHktRtsCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PC = BoundPlayerController.Get())
	{
		if (IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(PC))
		{
			if (WheelInputHandle.IsValid())
			{
				Interaction->OnWheelInput().Remove(WheelInputHandle);
				WheelInputHandle.Reset();
			}
			if (SubjectChangedHandle.IsValid())
			{
				Interaction->OnSubjectChanged().Remove(SubjectChangedHandle);
				SubjectChangedHandle.Reset();
			}
		}
	}
	BoundPlayerController.Reset();

	Super::EndPlay(EndPlayReason);
}

void AHktRtsCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	HandleCameraEdgeScroll(DeltaTime);
	UpdateFollowTarget(DeltaTime);
}

void AHktRtsCameraPawn::HandleZoom(float Value)
{
	Zoom(Value);
}

void AHktRtsCameraPawn::Zoom(float AxisValue)
{
	if (SpringArm && AxisValue != 0.0f)
	{
		SpringArm->TargetArmLength = FMath::Clamp(
			SpringArm->TargetArmLength - AxisValue * ZoomSpeed, MinZoom, MaxZoom);
	}
}

void AHktRtsCameraPawn::HandleCameraEdgeScroll(float DeltaTime)
{
	APlayerController* PC = GetController<APlayerController>();
	if (!PC) return;

	int32 ViewportSizeX, ViewportSizeY;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0) return;

	float MousePosX, MousePosY;
	if (PC->GetMousePosition(MousePosX, MousePosY))
	{
		FVector DirectionToMove = FVector::ZeroVector;
		const float EdgeX = ViewportSizeX * EdgeScrollThickness;
		const float EdgeY = ViewportSizeY * EdgeScrollThickness;

		if (MousePosX <= EdgeX)           DirectionToMove.Y = -1.0f;
		else if (MousePosX >= ViewportSizeX - EdgeX) DirectionToMove.Y = 1.0f;
		if (MousePosY <= EdgeY)           DirectionToMove.X = 1.0f;
		else if (MousePosY >= ViewportSizeY - EdgeY) DirectionToMove.X = -1.0f;

		if (!DirectionToMove.IsZero())
		{
			DirectionToMove.Normalize();
			AddActorWorldOffset(DirectionToMove * CameraScrollSpeed * DeltaTime);
		}
	}
}

void AHktRtsCameraPawn::OnSubjectChanged(FHktEntityId EntityId)
{
	if (EntityId == InvalidEntityId)
	{
		bFollowNewSpawn = true;
		FollowTargetEntityId = InvalidEntityId;
	}
	else
	{
		bFollowNewSpawn = false;
		FollowTargetEntityId = EntityId;
	}
}

void AHktRtsCameraPawn::UpdateFollowTarget(float DeltaTime)
{
	if (!bFollowNewSpawn) return;

	APlayerController* PC = GetController<APlayerController>();
	if (!PC) return;

	UHktPresentationSubsystem* Sub = UHktPresentationSubsystem::Get(PC);
	if (!Sub) return;

	const FHktPresentationState& State = Sub->GetState();

	// 이번 프레임에 새로 스폰된 엔터티가 있으면 그중 하나를 따라갈 대상으로 설정
	if (State.SpawnedThisFrame.Num() > 0)
	{
		FollowTargetEntityId = State.SpawnedThisFrame.Last();
	}

	if (FollowTargetEntityId == InvalidEntityId) return;

	const FHktEntityPresentation* E = State.Get(FollowTargetEntityId);
	if (!E || !E->IsAlive())
	{
		FollowTargetEntityId = InvalidEntityId;
		return;
	}

	FVector TargetLoc = E->Transform.Location.Get();
	FVector CurrentLoc = GetActorLocation();
	FVector NewLoc = FMath::VInterpTo(CurrentLoc, FVector(TargetLoc.X, TargetLoc.Y, CurrentLoc.Z), DeltaTime, FollowInterpSpeed);
	SetActorLocation(NewLoc);
}
