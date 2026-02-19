// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFileDatabaseComponent.h"
#include "HktRuntimeConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

// ============================================================================
// SaveGame Custom Serialization
// ============================================================================

void UHktPlayerSaveGame::Serialize(FArchive& Ar)
{
	// 1. 부모의 Serialize를 호출하여 UPROPERTY 매크로가 붙은 멤버들을 먼저 자동 직렬화합니다.
	Super::Serialize(Ar);

	// 2. UPROPERTY가 아니어서 직렬화되지 않은 순수 C++ 구조체 배열들을 수동으로 직렬화합니다.
	// 작성해주신 friend FArchive& operator<< 가 있기 때문에, 
	// TArray 전체를 아래처럼 한 줄로 직렬화(저장 및 로드)할 수 있습니다.
	Ar << PlayerRecord.ActiveEvents;

	// 추가로 USTRUCT가 아닌 다른 배열(예: EntityStates)이 있다면 같은 방식으로 직렬화하면 됩니다.
	Ar << PlayerRecord.EntityStates; 
}

UHktFileDatabaseComponent::UHktFileDatabaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DefaultVisualTag = FGameplayTag::RequestGameplayTag(TEXT("Visual.Character.Default"), false);
	DefaultFlowTag = FGameplayTag::RequestGameplayTag(TEXT("Flow.Character.Default"), false);
}

void UHktFileDatabaseComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UHktFileDatabaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const auto& Pair : CachedRecords)
	{
		SaveToSlot(Pair.Key, Pair.Value, [](bool) {});
	}
	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// 슬롯 관리 (Slot Management)
// ============================================================================

FString UHktFileDatabaseComponent::GetSaveSlotName(int64 PlayerUid)
{
	// 슬롯 이름 형식: "Player_{UID}"
	// Saved/SaveGames/Player_{UID}.sav 파일을 생성합니다.
	return FString::Printf(TEXT("Player_%lld"), PlayerUid);
}

// ============================================================================
// SaveGame 로드/저장 로직 (SaveGame Load/Save Logic)
// ============================================================================

void UHktFileDatabaseComponent::LoadFromSlot(int64 PlayerUid, TFunction<void(TOptional<FHktPlayerRecord>)> Callback)
{
	FString SlotName = GetSaveSlotName(PlayerUid);

	// SaveGame이 존재하는지 확인
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UHktPlayerSaveGame* LoadedGame = Cast<UHktPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (LoadedGame)
		{
			UE_LOG(LogTemp, Log, TEXT("[FileDatabase] Loaded SaveGame for player: %lld"), PlayerUid);
			Callback(TOptional<FHktPlayerRecord>(LoadedGame->PlayerRecord));
			return;
		}
	}

	// 찾을 수 없음 - 신규 플레이어로 간주
	Callback(TOptional<FHktPlayerRecord>());
}

void UHktFileDatabaseComponent::SaveToSlot(int64 PlayerUid, const FHktPlayerRecord& Record, TFunction<void(bool bSuccess)> Callback)
{
	FString SlotName = GetSaveSlotName(PlayerUid);

	UHktPlayerSaveGame* SaveGameInstance = Cast<UHktPlayerSaveGame>(UGameplayStatics::CreateSaveGameObject(UHktPlayerSaveGame::StaticClass()));
	if (SaveGameInstance)
	{
		SaveGameInstance->PlayerRecord = Record;

		if (UGameplayStatics::SaveGameToSlot(SaveGameInstance, SlotName, 0))
		{
			UE_LOG(LogTemp, Log, TEXT("[FileDatabase] Saved SaveGame for player: %lld"), PlayerUid);
			Callback(true);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[FileDatabase] Failed to write SaveGame to slot: %s"), *SlotName);
			Callback(false);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[FileDatabase] Failed to create SaveGame instance for player: %lld"), PlayerUid);
		Callback(false);
	}
}

// ============================================================================
// IHktWorldDatabase 구현
// ============================================================================

void UHktFileDatabaseComponent::LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(TUniquePtr<FHktPlayerRecord>)> InCallback)
{
	if (FHktPlayerRecord* Cached = CachedRecords.Find(InPlayerUid))
	{
		InCallback(MakeUnique<FHktPlayerRecord>(*Cached));
		return;
	}

	LoadFromSlot(InPlayerUid, [this, InPlayerUid, InCallback](TOptional<FHktPlayerRecord> Loaded)
	{
		if (Loaded.IsSet())
		{
			FHktPlayerRecord& Record = Loaded.GetValue();
			Record.PlayerUid = InPlayerUid;

			// 기존 레코드에 월드 진입 이벤트가 없으면 추가 (재진입 시)
			FGameplayTag InWorldTag = FGameplayTag::RequestGameplayTag(TEXT("State.Player.InWorld"), false);
			bool bHasInWorldEvent = false;
			for (const FHktEvent& Event : Record.ActiveEvents)
			{
				if (Event.EventTag == InWorldTag)
				{
					bHasInWorldEvent = true;
					break;
				}
			}

			if (!bHasInWorldEvent)
			{
				FHktEvent EnterWorldEvent;
				EnterWorldEvent.EventTag = InWorldTag;
				EnterWorldEvent.SourceEntity = static_cast<FHktEntityId>(InPlayerUid);
				EnterWorldEvent.TargetEntity = InvalidEntityId;
				EnterWorldEvent.Location = Record.LastPosition;
				EnterWorldEvent.Param0 = static_cast<int32>(InPlayerUid & 0xFFFFFFFF);
				EnterWorldEvent.Param1 = static_cast<int32>((InPlayerUid >> 32) & 0xFFFFFFFF);
				Record.ActiveEvents.Add(EnterWorldEvent);
			}

			CachedRecords.Add(InPlayerUid, Record);
			InCallback(MakeUnique<FHktPlayerRecord>(MoveTemp(Record)));
		}
		else
		{
			// 신규 레코드 생성
			FHktPlayerRecord NewRecord;
			NewRecord.PlayerUid = InPlayerUid;
			NewRecord.CreatedTime = FDateTime::UtcNow();
			NewRecord.LastLoginTime = NewRecord.CreatedTime;

			// 초기 월드 진입 이벤트 추가
			FHktEvent EnterWorldEvent;
			EnterWorldEvent.EventTag = FGameplayTag::RequestGameplayTag(TEXT("State.Player.InWorld"), false);
			EnterWorldEvent.SourceEntity = static_cast<FHktEntityId>(InPlayerUid);
			EnterWorldEvent.TargetEntity = InvalidEntityId;
			EnterWorldEvent.Location = FVector::ZeroVector;
			EnterWorldEvent.Param0 = static_cast<int32>(InPlayerUid & 0xFFFFFFFF);
			EnterWorldEvent.Param1 = static_cast<int32>((InPlayerUid >> 32) & 0xFFFFFFFF);
			NewRecord.ActiveEvents.Add(EnterWorldEvent);

			CachedRecords.Add(InPlayerUid, NewRecord);
			InCallback(MakeUnique<FHktPlayerRecord>(MoveTemp(NewRecord)));
		}
	});
}

void UHktFileDatabaseComponent::SavePlayerRecordAsync(FHktPlayerRecord InRecord)
{
	int64 Uid = InRecord.PlayerUid;
	CachedRecords.Add(Uid, InRecord);

	SaveToSlot(Uid, InRecord, [Uid](bool bSuccess)
	{
		if (!bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FileDatabase] Save failed for PlayerUid=%lld"), Uid);
		}
	});
}