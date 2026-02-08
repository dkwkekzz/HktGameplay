#pragma once

#include "CoreMinimal.h"

class IHktSubjectSelectionPolicy;
class IHktTargetSelectionPolicy;
class IHktCommandContainer;
class IHktIntentBuilder;
struct FHktFrameBatch;

class IHktClientRule
{
public:
    virtual ~IHktClientRule() = default;
    virtual void OnUserEvent_LoginButtonClick() = 0;
    virtual void OnUserEvent_SubjectInputAction(const IHktSubjectSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_TargetInputAction(const IHktTargetSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) = 0;
    virtual void OnReceived_FrameBatch(const FHktFrameBatch& InBatch, IHktWorldState& InState, IHktSimulator& InSimulator) = 0;
};
