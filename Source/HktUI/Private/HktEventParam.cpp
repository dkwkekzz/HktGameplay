// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktEventParam.h"

void UHktEventParam::Complete(bool bSuccess)
{
	if (bIsCompleted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktEventParam] Already completed, ignoring duplicate call"));
		return;
	}

	bIsCompleted = true;
	bIsSuccess = bSuccess;
	OnCompleted.Broadcast(this, bSuccess);
}

void UHktEventParam::Fail(const FString& InErrorMessage)
{
	ErrorMessage = InErrorMessage;
	Complete(false);
}
