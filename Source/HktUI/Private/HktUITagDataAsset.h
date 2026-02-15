// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "HktTagDataAsset.h"
#include "HktUITagDataAsset.generated.h"

class IHktUIView;
class UHktUIAnchorStrategy;

/**
 * UI 구성을 위한 DataAsset (Factory Method 포함).
 * HktAsset의 UHktTagDataAsset을 상속하여 태그 기반 비동기 로드와 연동합니다.
 */
UCLASS(Abstract, BlueprintType)
class HKTUI_API UHktUITagDataAsset : public UHktTagDataAsset
{
	GENERATED_BODY()

public:
	/** 이 위젯을 식별하는 태그 (에디터에서 설정, IdentifierTag와 동기화 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "Hkt|UI")
	FGameplayTag WidgetTag;

	/** 기본 배치 전략 클래스 (에디터에서 설정 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "Hkt|UI")
	TSubclassOf<UHktUIAnchorStrategy> DefaultAnchorStrategyClass;

	/** Factory Method: 구체적인 View 생성은 자식 클래스에서 구현 */
	virtual TSharedPtr<IHktUIView> CreateView() const PURE_VIRTUAL(UHktUITagDataAsset::CreateView, return nullptr;);

	/** Helper: 전략 객체 생성 (DefaultAnchorStrategyClass 기반) */
	virtual UHktUIAnchorStrategy* CreateStrategy(UObject* Outer) const;
};
