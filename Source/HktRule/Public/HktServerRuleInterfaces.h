// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HktCoreDefs.h"
#include "HktCoreEvents.h"
#include "HktCoreSimulator.h"
#include "HktWorldState.h"
#include "HktBagTypes.h"
#include "Containers/ArrayView.h"
#include "HktServerRuleInterfaces.generated.h"

// Forward declarations
class AActor;
class IHktRelevancyGraph;

/**
 * FHktPlayerRecord - 플레이어의 영구 저장 데이터
 */
USTRUCT(BlueprintType)
struct HKTRULE_API FHktPlayerRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	int64 PlayerUid = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	FDateTime LastLoginTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	FDateTime CreatedTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	FVector LastPosition;

	// TODO: ...
	TArray<FHktEvent> ActiveEvents;
	TArray<FHktEntityState> EntityStates;

	/** 가방 아이템 (DB 저장/복원) */
	TArray<FHktBagItem> BagItems;

	FHktPlayerRecord()
	{
		CreatedTime = FDateTime::UtcNow();
		LastLoginTime = CreatedTime;
	}

	bool IsValid() const { return PlayerUid != 0; }
	bool HasEntities() const { return EntityStates.Num() > 0; }
};

//=============================================================================
// IHktPlayerBag — 플레이어 가방 추상 인터페이스
//=============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktPlayerBag : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktPlayerBag
{
	GENERATED_BODY()
public:
	virtual const FHktBagState& GetBagState() const = 0;
	virtual bool StoreToBag(const FHktBagItem& InItem, int32& OutBagSlot) = 0;
	virtual bool TakeFromBag(int32 BagSlot, FHktBagItem& OutItem) = 0;
	virtual void RestoreFromRecord(const TArray<FHktBagItem>& InBagItems, int32 InCapacity = 20) = 0;
	virtual TArray<FHktBagItem> ExportForRecord() const = 0;
	virtual void SendFullSync() = 0;
};

//=============================================================================
// IHktWorldPlayer
//=============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktWorldPlayer : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktWorldPlayer
{
	GENERATED_BODY()
public:
	virtual int64 GetPlayerUid() const = 0;
	virtual AActor* GetOwnerActor() const = 0;
	virtual bool IsInitialized() const = 0;
	virtual void InvalidatePlayerUidCache() = 0;

	/** 플레이어 가방 접근 (없으면 nullptr) */
	virtual IHktPlayerBag* GetPlayerBag() const { return nullptr; }
};

// ============================================================================
// IHktAuthoritySimulator — 서버 전용 시뮬레이터 (Determinism + ExportPlayerState 등)
// ============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktAuthoritySimulator : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktAuthoritySimulator
{
	GENERATED_BODY()
public:
	/** 서버 전용: 플레이어 상태 내보내기 (저장/전송용) */
	virtual void AdvanceFrame(const FHktSimulationEvent& InEvent) = 0;
	virtual const FHktWorldState& GetWorldState() const = 0;
	virtual FHktPlayerState ExportPlayerState(int64 OwnerHash) const = 0;
};

//=============================================================================
// IHktRelevancyGroup — 서버 그룹 = 권위 시뮬레이터 + 플레이어 목록 (IHktAuthoritySimulator 상속)
//=============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktRelevancyGroup : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktRelevancyGroup
{
	GENERATED_BODY()
public:
	virtual const TArray<int64>& GetPlayerUids() const = 0;
	virtual const TArray<IHktWorldPlayer*>& GetCachedWorldPlayers() const = 0;
	virtual const IHktAuthoritySimulator& GetSimulator() const = 0;
	virtual IHktAuthoritySimulator& GetSimulator() = 0;
};

//=============================================================================
// IHktRelevancyGraph
//=============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktRelevancyGraph : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktRelevancyGraph
{
	GENERATED_BODY()
public:
	virtual void RegisterPlayer(IHktWorldPlayer* Player, int32 GroupIndex) = 0;
	virtual void UnregisterPlayer(int64 PlayerUid) = 0;
	virtual void UpdateRelevancy() = 0;
	virtual IHktWorldPlayer* GetWorldPlayer(int64 PlayerUid) const = 0;
	virtual int32 GetWorldPlayerCount() const = 0;
	virtual int32 NumRelevancyGroup() const = 0;
	virtual IHktRelevancyGroup& GetRelevancyGroup(int32 Index) = 0;
	virtual const IHktRelevancyGroup& GetRelevancyGroup(int32 Index) const = 0;
	virtual int32 GetRelevancyGroupIndex(int64 PlayerUid) const = 0;
	virtual int32 CalculateRelevancyGroupIndex(FVector PlayerPos) const = 0;
};

//=============================================================================
// IHktWorldDatabase
//=============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktWorldDatabase : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktWorldDatabase
{
	GENERATED_BODY()
public:
	virtual void LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(const FHktPlayerRecord&)> InCallback) = 0;
	virtual void SavePlayerRecordAsync(int64 InPlayerUid, FHktPlayerState&& InState) = 0;
	virtual const FHktPlayerRecord* GetCachedPlayerRecord(int64 InPlayerUid) const = 0;
};

//=============================================================================
// IHktFrameManager
//=============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktFrameManager : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktFrameManager
{
	GENERATED_BODY()
public:
	virtual bool IsInitialized() const = 0;
	virtual int64 GetFrameNumber() const = 0;
	virtual void AdvanceFrame() = 0;
};

//=============================================================================
// IHktPrincipal / IHktAuthenticator
//=============================================================================
UINTERFACE(MinimalAPI, BlueprintType)
class UHktPrincipal : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktPrincipal
{
	GENERATED_BODY()
public:
	virtual const FString& GetLoginID() const = 0;
	virtual const FString& GetLoginPW() const = 0;
	virtual const FString& GetAuthenticationToken() const = 0;
	virtual bool IsAuthenticated() const = 0;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UHktAuthenticator : public UInterface { GENERATED_BODY() };

class HKTRULE_API IHktAuthenticator
{
	GENERATED_BODY()
public:
	virtual void Authenticate(const FString& ID, const FString& PW, TFunction<void(bool bSuccess, const FString& Token)> ResultCallback) = 0;
	virtual void Deauthenticate(const FString& Token) = 0;
};

/** 그룹별 이벤트 전송 정보 (신입 제외 기존 플레이어에게 배치 전달) */
struct FGroupEventSend
{
	FHktSimulationEvent Batch;
	TArray<IHktWorldPlayer*> Entered;
	const TArray<IHktWorldPlayer*>* Existing;
	const FHktWorldState* NewState;
};

struct FHktEventGameModeTickResult
{
	TArray<FGroupEventSend> EventSends;
};

// ============================================================================
// FHktSlotRequest — 슬롯 커맨드 요청 (C2S)
// ============================================================================

struct HKTRULE_API FHktSlotRequest
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

	friend FArchive& operator<<(FArchive& Ar, FHktSlotRequest& R)
	{
		Ar << R.SlotIndex << R.SourceEntity << R.TargetEntity << R.TargetLocation;
		return Ar;
	}
};

// ============================================================================
// EHktItemAction — 아이템 요청 액션 타입
// ============================================================================

enum class EHktItemAction : uint8
{
	Pickup     = 0,
	Activate   = 1,
	Deactivate = 2,
	Drop       = 3,
};

// ============================================================================
// FHktItemRequest — 아이템 상호작용 요청 (C2S)
// ============================================================================

struct HKTRULE_API FHktItemRequest
{
	EHktItemAction Action = EHktItemAction::Pickup;
	FHktEntityId SourceEntity = InvalidEntityId;
	FHktEntityId TargetEntity = InvalidEntityId;
	int32 Param0 = 0;  // Activate: ActionSlot

	FString ToString() const
	{
		return FString::Printf(TEXT("Action=%d Src=%d Tgt=%d Param0=%d"),
			static_cast<uint8>(Action), SourceEntity, TargetEntity, Param0);
	}

	friend FArchive& operator<<(FArchive& Ar, FHktItemRequest& R)
	{
		uint8 ActionByte = static_cast<uint8>(R.Action);
		Ar << ActionByte << R.SourceEntity << R.TargetEntity << R.Param0;
		if (Ar.IsLoading()) R.Action = static_cast<EHktItemAction>(ActionByte);
		return Ar;
	}
};

// ============================================================================
// FHktMoveRequest — 이동 요청 (C2S)
// ============================================================================

struct HKTRULE_API FHktMoveRequest
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

	friend FArchive& operator<<(FArchive& Ar, FHktMoveRequest& R)
	{
		Ar << R.SourceEntity << R.TargetEntity << R.Location;
		return Ar;
	}
};

// ============================================================================
// EHktBagAction — 가방 요청 액션 타입
// ============================================================================

enum class EHktBagAction : uint8
{
	StoreFromSlot = 0,    // ItemSlot → Bag (공개 엔티티를 가방으로)
	RestoreToSlot = 1,    // Bag → ItemSlot (가방에서 공개 엔티티로)
	Discard       = 2,    // Bag → Ground (가방에서 바닥으로)
};

// ============================================================================
// FHktBagRequest — 가방 상호작용 요청 (C2S)
// ============================================================================

struct HKTRULE_API FHktBagRequest
{
	EHktBagAction Action = EHktBagAction::StoreFromSlot;
	FHktEntityId SourceEntity = InvalidEntityId;   // 캐릭터
	int32 BagSlot = -1;                            // 가방 슬롯 (RestoreToSlot/Discard)
	int32 ActionSlot = -1;                         // ItemSlot 인덱스 (StoreFromSlot: 출발, RestoreToSlot: 도착)

	FString ToString() const
	{
		return FString::Printf(TEXT("Action=%d Src=%d BagSlot=%d ActionSlot=%d"),
			static_cast<uint8>(Action), SourceEntity, BagSlot, ActionSlot);
	}

	friend FArchive& operator<<(FArchive& Ar, FHktBagRequest& R)
	{
		uint8 ActionByte = static_cast<uint8>(R.Action);
		Ar << ActionByte << R.SourceEntity << R.BagSlot << R.ActionSlot;
		if (Ar.IsLoading()) R.Action = static_cast<EHktBagAction>(ActionByte);
		return Ar;
	}
};

//=============================================================================
// IHktServerRule
//=============================================================================
class HKTRULE_API IHktServerRule
{
public:
	virtual ~IHktServerRule() = default;

	/** 컨텍스트 바인딩 — 룰 내부에서 인터페이스 캐싱 (item 2) */
	virtual void BindContext(
		IHktFrameManager* InFrame,
		IHktRelevancyGraph* InGraph,
		IHktWorldDatabase* InDB) {}

	virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) {}
	virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) {}

	/** 슬롯 커맨드 요청 수신 — 서버가 WorldState에서 EventTag 해석 */
	virtual void OnReceived_SlotRequest(const FHktSlotRequest& InRequest, const IHktWorldPlayer& InPlayer) {}

	/** 이동 요청 수신 — 서버가 Move EventTag 매핑 */
	virtual void OnReceived_MoveRequest(const FHktMoveRequest& InRequest, const IHktWorldPlayer& InPlayer) {}

	/** 아이템 상호작용 요청 수신 — 서버가 ItemState 검증 후 EventTag 매핑 */
	virtual void OnReceived_ItemRequest(const FHktItemRequest& InRequest, const IHktWorldPlayer& InPlayer) {}

	/** 가방 요청 수신 — 서버가 Bag ↔ Entity 전환 처리 (Bag 상태 변경이 필요하므로 non-const) */
	virtual void OnReceived_BagRequest(const FHktBagRequest& InRequest, IHktWorldPlayer& InPlayer) {}

	/** 액터 이벤트 — 내부 캐싱된 DB 사용 (item 1, 2) */
	virtual void OnEvent_GameModePostLogin(const IHktWorldPlayer& InPlayer) {}
	virtual void OnEvent_GameModeLogout(const IHktWorldPlayer& InPlayer) {}

	/** 틱 — 내부 캐싱된 컨텍스트 사용, 결과 구조체 반환 (item 1, 2, 6) */
	virtual FHktEventGameModeTickResult OnEvent_GameModeTick(float InDeltaTime) { return {}; }
};

namespace HktRule
{
	HKTRULE_API IHktServerRule* GetServerRule(UWorld* World);
}
