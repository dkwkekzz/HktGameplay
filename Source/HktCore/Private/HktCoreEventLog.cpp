// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreEventLog.h"

DEFINE_LOG_CATEGORY(LogHktEvent);

FHktCoreEventLog& FHktCoreEventLog::Get()
{
	static FHktCoreEventLog Instance;
	return Instance;
}

void FHktCoreEventLog::Log(const TCHAR* Category, const FString& Message,
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

	KnownCategories.Add(Category);
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

TArray<FString> FHktCoreEventLog::GetCategories() const
{
	FScopeLock ScopeLock(&Lock);
	return KnownCategories.Array();
}
