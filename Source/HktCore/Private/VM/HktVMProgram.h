// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktFlowTypes.h"

/**
 * FHktVMProgram - 컴파일된 바이트코드 프로그램 (불변, 공유 가능)
 */
struct HKTCORE_API FHktVMProgram
{
    FGameplayTag Tag;
    TArray<FInstruction> Code;
    TArray<int32> Constants;
    TArray<FString> Strings;
    TArray<int32> LineNumbers;

    /** 같은 SourceEntity에 동일 EventTag VM이 이미 있으면 기존 것을 취소 */
    bool bCancelOnDuplicate = false;

    bool IsValid() const { return Code.Num() > 0; }
    int32 CodeSize() const { return Code.Num(); }
};

/**
 * FHktVMProgramRegistry - EventTag → Program 매핑 관리
 */
class HKTCORE_API FHktVMProgramRegistry
{
public:
    static FHktVMProgramRegistry& Get();

    const FHktVMProgram* FindProgram(const FGameplayTag& Tag) const;
    void RegisterProgram(TSharedRef<FHktVMProgram> Program);
    void Clear();

private:
    FHktVMProgramRegistry() = default;

    TMap<FGameplayTag, TSharedRef<FHktVMProgram>> Programs;
    mutable FRWLock Lock;
};

