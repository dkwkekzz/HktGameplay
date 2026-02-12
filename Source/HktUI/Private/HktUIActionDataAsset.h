// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktTagDataAsset.h"
#include "HktUITypes.h"
#include "HktUIActionDataAsset.generated.h"

class UUserWidget;

/**
 * UI 액션 DataAsset
 *
 * 태그에 대응하는 UI 액션과 위젯 부착 방식을 정의합니다.
 *
 * AttachTarget:
 *   Viewport → AddToViewport (로그인, 인벤토리 등)
 *   Widget   → ParentWidgetTag로 지정한 기존 위젯의 자식으로 부착
 *   Entity   → EntityId 기반으로 월드 위치를 조회하여 WidgetComponent로 부착
 *              Actor든 MassEntity든 상관없음 (IHktUserEventDispatcher가 위치 제공)
 */
UCLASS(BlueprintType)
class HKTUI_API UHktUIActionDataAsset : public UHktTagDataAsset
{
	GENERATED_BODY()

public:
	// === 액션 ===

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	EHktUIActionType ActionType = EHktUIActionType::DispatchEvent;

	// === 위젯 부착 ===

	/** 위젯을 어디에 붙일 것인가 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attach")
	EHktUIAttachTarget AttachTarget = EHktUIAttachTarget::Viewport;

	/** Widget 부착 시 부모 위젯 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attach",
		meta = (EditCondition = "AttachTarget == EHktUIAttachTarget::Widget"))
	FGameplayTag ParentWidgetTag;

	/** Entity 부착 시 WidgetComponent 오프셋 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attach",
		meta = (EditCondition = "AttachTarget == EHktUIAttachTarget::Entity"))
	FVector EntityAttachOffset = FVector(0.0f, 0.0f, 120.0f);

	/** Entity 부착 시 WidgetComponent DrawSize */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attach",
		meta = (EditCondition = "AttachTarget == EHktUIAttachTarget::Entity"))
	FVector2D EntityDrawSize = FVector2D(100.0f, 30.0f);

	// === 위젯 생성 ===

	/** CreateWidget 시 생성할 위젯 클래스 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget",
		meta = (EditCondition = "ActionType == EHktUIActionType::CreateWidget || ActionType == EHktUIActionType::CreateWidgetAndDispatch"))
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** DestroyWidget 시 파괴할 위젯 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget",
		meta = (EditCondition = "ActionType == EHktUIActionType::DestroyWidget"))
	FGameplayTag TargetWidgetTag;

	// === 이벤트 디스패치 ===

	/** DispatchEvent 시 PlayerController에 전달할 이벤트 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dispatch",
		meta = (EditCondition = "ActionType == EHktUIActionType::DispatchEvent || ActionType == EHktUIActionType::CreateWidgetAndDispatch"))
	FGameplayTag DispatchEventTag;
};
