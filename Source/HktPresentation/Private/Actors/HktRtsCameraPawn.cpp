// Fill out your copyright notice in the Description page of Project Settings.


#include "HktRtsCameraPawn.h"
#include "HktPresentationSubsystem.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

AHktRtsCameraPawn::AHktRtsCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// 스프링 암 생성
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->bDoCollisionTest = false; // RTS 카메라는 지형 충돌 무시
	SpringArm->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f)); // RTS 뷰 각도
	SpringArm->TargetArmLength = 2000.0f; // 기본 줌 거리

	// 카메라 생성
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
}

void AHktRtsCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	if (UHktPresentationSubsystem* Sub = UHktPresentationSubsystem::Get(GetController<APlayerController>()))
	{
		PresentationSubsystem = Sub;
		Sub->OnWheelInput.AddUObject(this, &AHktRtsCameraPawn::HandleZoom);
	}
}

void AHktRtsCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHktPresentationSubsystem* Sub = PresentationSubsystem.Get())
	{
		Sub->OnWheelInput.RemoveAll(this);
	}
	PresentationSubsystem.Reset();

	Super::EndPlay(EndPlayReason);
}

void AHktRtsCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	HandleCameraEdgeScroll(DeltaTime);
}

void AHktRtsCameraPawn::HandleZoom(float Value)
{
	Zoom(Value);
}

void AHktRtsCameraPawn::Zoom(float AxisValue)
{
	if (SpringArm && AxisValue != 0.0f)
	{
		// 휠을 내리면(AxisValue < 0) 줌 아웃(거리 증가), 올리면(AxisValue > 0) 줌 인(거리 감소)
		SpringArm->TargetArmLength = FMath::Clamp(SpringArm->TargetArmLength - AxisValue * ZoomSpeed, MinZoom, MaxZoom);
	}
}

void AHktRtsCameraPawn::HandleCameraEdgeScroll(float DeltaTime)
{
	APlayerController* PlayerController = GetController<APlayerController>();
	if (!PlayerController) return;

	int32 ViewportSizeX, ViewportSizeY;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0) return;

	float MousePosX, MousePosY;
	if (PlayerController->GetMousePosition(MousePosX, MousePosY))
	{
		FVector2D MousePosition(MousePosX, MousePosY);
		FVector DirectionToMove = FVector::ZeroVector;

		float EdgeX = ViewportSizeX * EdgeScrollThickness;
		float EdgeY = ViewportSizeY * EdgeScrollThickness;

		if (MousePosX <= EdgeX)
		{
			DirectionToMove.Y = -1.0f; // 왼쪽
		}
		else if (MousePosX >= ViewportSizeX - EdgeX)
		{
			DirectionToMove.Y = 1.0f; // 오른쪽
		}

		if (MousePosY <= EdgeY)
		{
			DirectionToMove.X = 1.0f; // 위쪽
		}
		else if (MousePosY >= ViewportSizeY - EdgeY)
		{
			DirectionToMove.X = -1.0f; // 아래쪽
		}

		if (!DirectionToMove.IsZero())
		{
			DirectionToMove.Normalize();
			AddActorWorldOffset(DirectionToMove * CameraScrollSpeed * DeltaTime);
		}
	}
}

void AHktRtsCameraPawn::OnMoveCameraToMinimapLocation(const FVector2D& NormalizedLocation)
{
	float WorldSizeX = 10000.0f;
	float WorldSizeY = 10000.0f;

	FVector CurrentLocation = GetActorLocation();
	FVector TargetLocation((1.f - NormalizedLocation.Y) * WorldSizeX, NormalizedLocation.X * WorldSizeY, CurrentLocation.Z);

	SetActorLocation(TargetLocation);
}
