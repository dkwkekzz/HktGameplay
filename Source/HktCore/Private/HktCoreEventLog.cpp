// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreEventLog.h"

// ============================================================================
// 로그 카테고리 GameplayTag 정의
// ============================================================================

namespace HktLogTags
{
	UE_DEFINE_GAMEPLAY_TAG(Core,            "HktLog.Core");
	UE_DEFINE_GAMEPLAY_TAG(Core_Entity,     "HktLog.Core.Entity");
	UE_DEFINE_GAMEPLAY_TAG(Core_VM,         "HktLog.Core.VM");
	UE_DEFINE_GAMEPLAY_TAG(Core_Story,      "HktLog.Core.Story");
	UE_DEFINE_GAMEPLAY_TAG(Runtime,         "HktLog.Runtime");
	UE_DEFINE_GAMEPLAY_TAG(Runtime_Server,  "HktLog.Runtime.Server");
	UE_DEFINE_GAMEPLAY_TAG(Runtime_Client,  "HktLog.Runtime.Client");
	UE_DEFINE_GAMEPLAY_TAG(Runtime_Intent,  "HktLog.Runtime.Intent");
	UE_DEFINE_GAMEPLAY_TAG(Presentation,    "HktLog.Presentation");
	UE_DEFINE_GAMEPLAY_TAG(Asset,           "HktLog.Asset");
	UE_DEFINE_GAMEPLAY_TAG(Rule,            "HktLog.Rule");
	UE_DEFINE_GAMEPLAY_TAG(Story,           "HktLog.Story");
	UE_DEFINE_GAMEPLAY_TAG(UI,              "HktLog.UI");
	UE_DEFINE_GAMEPLAY_TAG(VFX,             "HktLog.VFX");
}

FHktCoreEventLog& FHktCoreEventLog::Get()
{
	static FHktCoreEventLog Instance;
	return Instance;
}

void FHktCoreEventLog::Log(const FGameplayTag& Category, const FString& Message,
                           FHktEntityId EntityId, FGameplayTag EventTag)
{
	if (!bActive)
	{
		return;
	}

	FScopeLock ScopeLock(&Lock);

	// 링 버퍼 초기화 (지연 할당)
	if (Entries.Num() == 0)
	{
		Entries.SetNum(MaxEntries);
	}

	const int32 Index = WriteIndex % MaxEntries;
	FHktLogEntry& Entry = Entries[Index];
	Entry.Timestamp = FPlatformTime::Seconds();
	Entry.FrameNumber = GFrameCounter;
	Entry.Category = Category;
	Entry.Message = Message;
	Entry.EntityId = EntityId;
	Entry.EventTag = EventTag;

	++WriteIndex;
	++Version;

	KnownCategories.AddTag(Category);
}

void FHktCoreEventLog::SetActive(bool bNewActive)
{
	FScopeLock ScopeLock(&Lock);
	bActive = bNewActive;
}

TArray<FHktLogEntry> FHktCoreEventLog::Consume(uint32& InOutReadIndex) const
{
	FScopeLock ScopeLock(&Lock);

	TArray<FHktLogEntry> Result;

	if (InOutReadIndex >= WriteIndex)
	{
		return Result;
	}

	// 읽기 시작점: WriteIndex가 MaxEntries를 넘어갔으면 오래된 데이터는 덮어쓰여짐
	uint32 StartIndex = InOutReadIndex;
	if (WriteIndex - StartIndex > (uint32)MaxEntries)
	{
		StartIndex = WriteIndex - MaxEntries;
	}

	const int32 Count = WriteIndex - StartIndex;
	Result.Reserve(Count);

	for (uint32 i = StartIndex; i < WriteIndex; ++i)
	{
		Result.Add(Entries[i % MaxEntries]);
	}

	InOutReadIndex = WriteIndex;
	return Result;
}

void FHktCoreEventLog::Clear()
{
	FScopeLock ScopeLock(&Lock);
	Entries.Reset();
	WriteIndex = 0;
	++Version;
}

FGameplayTagContainer FHktCoreEventLog::GetCategories() const
{
	FScopeLock ScopeLock(&Lock);
	return KnownCategories;
}
