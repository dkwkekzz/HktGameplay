// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFileDatabaseComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HktStoryTags.h"

// ============================================================================
// SaveGame Custom Serialization
// ============================================================================

void UHktPlayerSaveGame::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	//Ar << PlayerRecord.ActiveEvents;
	//Ar << PlayerRecord.EntityStates; 
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

void UHktFileDatabaseComponent::LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(const FHktPlayerRecord&)> InCallback)
{
	if (FHktPlayerRecord* Cached = CachedRecords.Find(InPlayerUid))
	{
		InCallback(*Cached);
		return;
	}

	LoadFromSlot(InPlayerUid, [this, InPlayerUid, InCallback](TOptional<FHktPlayerRecord> Loaded)
	{
		if (Loaded.IsSet())
		{
			FHktPlayerRecord& Record = Loaded.GetValue();
			Record.PlayerUid = InPlayerUid;

			// 기존 레코드에 월드 진입 이벤트가 없으면 추가 (재진입 시)
			const FGameplayTag InWorldTag = HktStoryTags::Story_PlayerInWorld;
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
				EnterWorldEvent.PlayerUid = InPlayerUid;
				Record.ActiveEvents.Add(EnterWorldEvent);
			}

			CachedRecords.Add(InPlayerUid, Record);
			InCallback(Record);
		}
		else
		{
			// 신규 레코드 생성
			FHktPlayerRecord& NewRecord = CachedRecords.Add(InPlayerUid);
			NewRecord.PlayerUid = InPlayerUid;
			NewRecord.CreatedTime = FDateTime::UtcNow();
			NewRecord.LastLoginTime = NewRecord.CreatedTime;

			// 초기 월드 진입 이벤트 추가
			FHktEvent EnterWorldEvent;
			EnterWorldEvent.EventTag = HktStoryTags::Story_PlayerInWorld;
			EnterWorldEvent.SourceEntity = static_cast<FHktEntityId>(InPlayerUid);
			EnterWorldEvent.TargetEntity = InvalidEntityId;
			EnterWorldEvent.Location = FVector::ZeroVector;
			EnterWorldEvent.PlayerUid = InPlayerUid;
			NewRecord.ActiveEvents.Add(EnterWorldEvent);

			InCallback(NewRecord);
		}
	});
}

void UHktFileDatabaseComponent::SavePlayerRecordAsync(int64 InPlayerUid, FHktPlayerState&& InState)
{
	// 기존 레코드 로드 또는 새로 생성
	FHktPlayerRecord* ExistingRecord = CachedRecords.Find(InPlayerUid);
	
	if (ExistingRecord)
	{
		// 기존 레코드 업데이트: ActiveEvents와 EntityStates만 이동
		ExistingRecord->ActiveEvents = MoveTemp(InState.ActiveEvents);
		ExistingRecord->EntityStates = MoveTemp(InState.OwnedEntities);
		// LastLoginTime, CreatedTime, LastPosition은 유지
		
		SaveToSlot(InPlayerUid, *ExistingRecord, [InPlayerUid](bool bSuccess)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("[FileDatabase] Save failed for PlayerUid=%lld"), InPlayerUid);
			}
		});
	}
	else
	{
		// 캐시에 없으면 파일에서 로드 시도
		// 람다에서 이동하기 위해 State를 이동 캡처
		LoadFromSlot(InPlayerUid, [this, InPlayerUid, State = MoveTemp(InState)](TOptional<FHktPlayerRecord> Loaded) mutable
		{
			FHktPlayerRecord& RecordToSave = CachedRecords.Add(InPlayerUid);
			
			if (Loaded.IsSet())
			{
				// 파일에서 로드된 레코드 사용
				RecordToSave = Loaded.GetValue();
				RecordToSave.PlayerUid = InPlayerUid;
			}
			else
			{
				// 신규 레코드 생성
				RecordToSave.PlayerUid = InPlayerUid;
				RecordToSave.CreatedTime = FDateTime::UtcNow();
				RecordToSave.LastLoginTime = RecordToSave.CreatedTime;
				RecordToSave.LastPosition = FVector::ZeroVector;
			}
			
			// ActiveEvents와 EntityStates 이동
			RecordToSave.ActiveEvents = MoveTemp(State.ActiveEvents);
			RecordToSave.EntityStates = MoveTemp(State.OwnedEntities);
			
			// 파일에 저장
			SaveToSlot(InPlayerUid, CachedRecords[InPlayerUid], [InPlayerUid](bool bSuccess)
			{
				if (!bSuccess)
				{
					UE_LOG(LogTemp, Warning, TEXT("[FileDatabase] Save failed for PlayerUid=%lld"), InPlayerUid);
				}
			});
		});
	}
}

const FHktPlayerRecord* UHktFileDatabaseComponent::GetCachedPlayerRecord(int64 InPlayerUid) const
{
	return CachedRecords.Find(InPlayerUid);
}