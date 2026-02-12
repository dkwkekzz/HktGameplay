// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "HktEventParam.generated.h"

/**
 * UHktEventParam
 *
 * UI → PlayerController 이벤트 전달 시 인자와 콜백을 담는 기본 클래스.
 * 서브클래스를 만들어 다양한 이벤트 파라미터를 유연하게 정의.
 */
UCLASS(BlueprintType, Blueprintable)
class HKTUI_API UHktEventParam : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEventCompleted, UHktEventParam*, Param, bool, bSuccess);

	UPROPERTY(BlueprintAssignable, Category = "Hkt|Event")
	FOnEventCompleted OnCompleted;

	UFUNCTION(BlueprintCallable, Category = "Hkt|Event")
	void Complete(bool bSuccess);

	UFUNCTION(BlueprintCallable, Category = "Hkt|Event")
	void Fail(const FString& InErrorMessage);

	UFUNCTION(BlueprintPure, Category = "Hkt|Event")
	bool IsCompleted() const { return bIsCompleted; }

	UFUNCTION(BlueprintPure, Category = "Hkt|Event")
	bool IsSuccess() const { return bIsSuccess; }

	UFUNCTION(BlueprintPure, Category = "Hkt|Event")
	FString GetErrorMessage() const { return ErrorMessage; }

protected:
	bool bIsCompleted = false;
	bool bIsSuccess = false;
	FString ErrorMessage;
};

/**
 * 로그인 이벤트 파라미터
 */
UCLASS(BlueprintType)
class HKTUI_API UHktLoginEventParam : public UHktEventParam
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Hkt|Login")
	FString UserId;

	UPROPERTY(BlueprintReadWrite, Category = "Hkt|Login")
	FString Password;

	UPROPERTY(BlueprintReadWrite, Category = "Hkt|Login")
	FString ResponseToken;
};
