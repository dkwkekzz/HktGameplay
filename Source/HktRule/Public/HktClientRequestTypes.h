// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreDefs.h"
#include "HktClientRequestTypes.generated.h"

// ============================================================================
// C2S 클라이언트 요청 패킷
//
// 클라이언트는 EventTag를 보내지 않고 입력 형태(enum+index)만 전송.
// 서버가 WorldState에서 실제 EventTag를 해석하여 FHktEvent를 생성.
// ============================================================================

// ============================================================================
// FHktClientSlotRequest — 슬롯 커맨드 요청
// ============================================================================

struct HKTRULE_API FHktClientSlotRequest
{
	int32 SlotIndex = 0;
	FHktEntityId SourceEntity = InvalidEntityId;
	FHktEntityId TargetEntity = InvalidEntityId;
	FVector TargetLocation = FVector::ZeroVector;

	FString ToString() const
	{
		return FString::Printf(TEXT("Slot=%d Src=%d Tgt=%d Loc=(%.0f,%.0f,%.0f)"),
			SlotIndex, SourceEntity, TargetEntity,
			TargetLocation.X, TargetLocation.Y, TargetLocation.Z);
	}

	friend FArchive& operator<<(FArchive& Ar, FHktClientSlotRequest& R)
	{
		Ar << R.SlotIndex << R.SourceEntity << R.TargetEntity << R.TargetLocation;
		return Ar;
	}
};

// ============================================================================
// FHktClientMoveRequest — 이동 요청
// ============================================================================

struct HKTRULE_API FHktClientMoveRequest
{
	FHktEntityId SourceEntity = InvalidEntityId;
	FHktEntityId TargetEntity = InvalidEntityId;
	FVector Location = FVector::ZeroVector;

	FString ToString() const
	{
		return FString::Printf(TEXT("Src=%d Tgt=%d Loc=(%.0f,%.0f,%.0f)"),
			SourceEntity, TargetEntity,
			Location.X, Location.Y, Location.Z);
	}

	friend FArchive& operator<<(FArchive& Ar, FHktClientMoveRequest& R)
	{
		Ar << R.SourceEntity << R.TargetEntity << R.Location;
		return Ar;
	}
};

// ============================================================================
// FHktRuntimeSlotRequest — 슬롯 요청 네트워크 래퍼
// ============================================================================

USTRUCT()
struct HKTRULE_API FHktRuntimeSlotRequest
{
	GENERATED_BODY()

	FHktClientSlotRequest Value;

	FHktRuntimeSlotRequest() = default;
	explicit FHktRuntimeSlotRequest(const FHktClientSlotRequest& In) : Value(In) {}

	FORCEINLINE operator FHktClientSlotRequest&() { return Value; }
	FORCEINLINE operator const FHktClientSlotRequest&() const { return Value; }

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Value;
		return bOutSuccess = true;
	}
};

template<>
struct TStructOpsTypeTraits<FHktRuntimeSlotRequest> : public TStructOpsTypeTraitsBase2<FHktRuntimeSlotRequest>
{
	enum { WithNetSerializer = true };
};

// ============================================================================
// FHktRuntimeMoveRequest — 이동 요청 네트워크 래퍼
// ============================================================================

USTRUCT()
struct HKTRULE_API FHktRuntimeMoveRequest
{
	GENERATED_BODY()

	FHktClientMoveRequest Value;

	FHktRuntimeMoveRequest() = default;
	explicit FHktRuntimeMoveRequest(const FHktClientMoveRequest& In) : Value(In) {}

	FORCEINLINE operator FHktClientMoveRequest&() { return Value; }
	FORCEINLINE operator const FHktClientMoveRequest&() const { return Value; }

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Value;
		return bOutSuccess = true;
	}
};

template<>
struct TStructOpsTypeTraits<FHktRuntimeMoveRequest> : public TStructOpsTypeTraitsBase2<FHktRuntimeMoveRequest>
{
	enum { WithNetSerializer = true };
};
