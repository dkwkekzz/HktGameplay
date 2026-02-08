// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

using FHktEntityId = int32;
constexpr FHktEntityId InvalidEntityId = INDEX_NONE;

using FHktWorldCell = FIntPoint;
const FHktWorldCell InvalidCell = FHktWorldCell(INT_MAX, INT_MAX);

using FHktFrameNumber = int64;