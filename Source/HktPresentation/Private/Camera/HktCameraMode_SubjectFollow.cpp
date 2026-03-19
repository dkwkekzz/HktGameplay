// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Camera/HktCameraMode_SubjectFollow.h"
#include "Actors/HktRtsCameraPawn.h"
#include "HktPresentationSubsystem.h"
#include "HktPresentationState.h"
#include "GameFramework/PlayerController.h"

void UHktCameraMode_SubjectFollow::OnActivate(AHktRtsCameraPawn* Pawn)
{
	if (!Pawn) return;

	// 현재 카메라와 대상 위치의 차이를 ManualOffset으로 설정하여 점프 방지
	if (SubjectEntityId != InvalidEntityId)
	{
		APlayerController* PC = Pawn->GetBoundPC();
		UHktPresentationSubsystem* Sub = PC ? UHktPresentationSubsystem::Get(PC) : nullptr;
		if (Sub)
		{
			const FHktEntityPresentation* E = Sub->GetState().Get(SubjectEntityId);
			if (E && E->IsAlive())
			{
				FVector EntityLoc = E->Location.Get();
				FVector CameraLoc = Pawn->GetActorLocation();
				ManualOffset = FVector(CameraLoc.X - EntityLoc.X, CameraLoc.Y - EntityLoc.Y, 0.0f);
				return;
			}
		}
	}

	ManualOffset = FVector::ZeroVector;
}

void UHktCameraMode_SubjectFollow::TickMode(AHktRtsCameraPawn* Pawn, float DeltaTime)
{
	if (!Pawn || SubjectEntityId == InvalidEntityId) return;

	APlayerController* PC = Pawn->GetBoundPC();
	if (!PC) return;

	UHktPresentationSubsystem* Sub = UHktPresentationSubsystem::Get(PC);
	if (!Sub) return;

	const FHktEntityPresentation* E = Sub->GetState().Get(SubjectEntityId);
	if (!E || !E->IsAlive())
	{
		// 대상이 사라지면 RtsFree 모드로 전환 요청
		SubjectEntityId = InvalidEntityId;
		Pawn->SetCameraMode(EHktCameraMode::RtsFree);
		return;
	}

	// Edge scroll 오프셋 업데이트
	HandleEdgeScrollOffset(Pawn, DeltaTime);

	// 오프셋 감쇄
	ManualOffset = FMath::VInterpTo(ManualOffset, FVector::ZeroVector, DeltaTime, OffsetDecaySpeed);

	// 대상 위치 + 오프셋으로 보간 이동
	FVector EntityLoc = E->Location.Get();
	FVector TargetLoc = FVector(EntityLoc.X + ManualOffset.X, EntityLoc.Y + ManualOffset.Y, Pawn->GetActorLocation().Z);
	FVector CurrentLoc = Pawn->GetActorLocation();
	FVector NewLoc = FMath::VInterpTo(CurrentLoc, TargetLoc, DeltaTime, FollowInterpSpeed);
	Pawn->SetActorLocation(NewLoc);
}

void UHktCameraMode_SubjectFollow::OnSubjectChanged(AHktRtsCameraPawn* Pawn, FHktEntityId EntityId)
{
	SubjectEntityId = EntityId;

	if (EntityId == InvalidEntityId)
	{
		ManualOffset = FVector::ZeroVector;
	}
}

void UHktCameraMode_SubjectFollow::HandleEdgeScrollOffset(AHktRtsCameraPawn* Pawn, float DeltaTime)
{
	APlayerController* PC = Pawn->GetBoundPC();
	if (!PC) return;

	int32 ViewportSizeX, ViewportSizeY;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0) return;

	float MousePosX, MousePosY;
	if (PC->GetMousePosition(MousePosX, MousePosY))
	{
		FVector Direction = FVector::ZeroVector;
		const float EdgeX = ViewportSizeX * EdgeScrollThickness;
		const float EdgeY = ViewportSizeY * EdgeScrollThickness;

		if (MousePosX <= EdgeX)                          Direction.Y = -1.0f;
		else if (MousePosX >= ViewportSizeX - EdgeX)     Direction.Y = 1.0f;
		if (MousePosY <= EdgeY)                          Direction.X = 1.0f;
		else if (MousePosY >= ViewportSizeY - EdgeY)     Direction.X = -1.0f;

		if (!Direction.IsZero())
		{
			Direction.Normalize();
			ManualOffset += Direction * EdgeScrollSpeed * DeltaTime;
		}
	}
}
