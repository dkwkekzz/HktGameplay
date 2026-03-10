// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreDataCollector.h"

FHktCoreDataCollector& FHktCoreDataCollector::Get()
{
	static FHktCoreDataCollector Instance;
	return Instance;
}

void FHktCoreDataCollector::SetValue(const FString& Category, const FString& Key, const FString& Value)
{
	FScopeLock ScopeLock(&Lock);
	Data.FindOrAdd(Category).SetValue(Key, Value);
	++Version;
}

TArray<TPair<FString, FString>> FHktCoreDataCollector::GetEntries(const FString& Category) const
{
	FScopeLock ScopeLock(&Lock);
	if (const FCategoryData* Cat = Data.Find(Category))
	{
		return Cat->Rows;
	}
	return {};
}

TArray<FString> FHktCoreDataCollector::GetCategories() const
{
	FScopeLock ScopeLock(&Lock);
	TArray<FString> Result;
	Data.GetKeys(Result);
	return Result;
}

void FHktCoreDataCollector::ClearCategory(const FString& Category)
{
	FScopeLock ScopeLock(&Lock);
	Data.Remove(Category);
	++Version;
}

void FHktCoreDataCollector::Clear()
{
	FScopeLock ScopeLock(&Lock);
	Data.Empty();
	++Version;
}
