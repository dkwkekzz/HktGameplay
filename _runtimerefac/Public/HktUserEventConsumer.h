// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HktUserEventConsumer.generated.h"

// ============================================================================
// IHktUserEventConsumer 인터페이스
// ============================================================================

struct FHktUserEvent
{
    FName Name;
    TArray<FVariant> Datas;

    FHktUserEvent(FName InEventName)
        : Name(InEventName)
    {}
};

UINTERFACE(MinimalAPI, BlueprintType)
class UHktUserEventConsumer : public UInterface
{
	GENERATED_BODY()
};

/**
 * IHktUserEventConsumer
 * 
 */
class HKTRUNTIME_API IHktUserEventConsumer
{
	GENERATED_BODY()

public:
    /** 유저 이벤트 수신 시 */
    virtual void OnUserEvent(const FHktUserEvent& Event) = 0;
};
