// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HktSelectable.h"
#include "HktCharacter.generated.h"

/**
 * IHktSelectable을 구현하는 테스트용 캐릭터 클래스.
 * 커서 선택 시 EntityId를 제공하여 Intent 시스템에서 사용됨.
 */
UCLASS()
class HKTPRESENTATION_API AHktCharacter : public ACharacter, public IHktSelectable
{
	GENERATED_BODY()

public:
	AHktCharacter();

	//-------------------------------------------------------------------------
	// IHktSelectable Interface
	//-------------------------------------------------------------------------
	
	/** 이 캐릭터의 EntityId를 반환 */
	virtual FHktEntityId GetEntityId() const override;
	
	/** 선택 가능 여부 반환 */
	virtual bool IsSelectable() const override;

	//-------------------------------------------------------------------------
	// Entity Management
	//-------------------------------------------------------------------------
	
	/** EntityId 설정 (Spawn 시 또는 서버에서 할당) */
	UFUNCTION(BlueprintCallable, Category = "Hkt|Character")
	void SetEntityId(FHktEntityId InEntityId);
	
	/** 선택 가능 여부 설정 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|Character")
	void SetSelectable(bool bInSelectable);

protected:
	/** 이 캐릭터의 고유 EntityId */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hkt|Character")
	FHktEntityId EntityId;
	
	/** 선택 가능 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|Character")
	bool bSelectable;
};
