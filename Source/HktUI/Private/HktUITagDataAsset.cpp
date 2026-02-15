// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUITagDataAsset.h"
#include "HktUIAnchorStrategy.h"

UHktUIAnchorStrategy* UHktUITagDataAsset::CreateStrategy(UObject* Outer) const
{
	if (!DefaultAnchorStrategyClass || !Outer) return nullptr;
	return NewObject<UHktUIAnchorStrategy>(Outer, DefaultAnchorStrategyClass);
}
