// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HktInsightProvider.generated.h"

/**
 * FHktInsightEntry - 단일 인사이트 항목
 *
 * Provider가 수집한 키-값 쌍 하나를 표현합니다.
 * Category를 통해 UI에서 그룹핑할 수 있습니다.
 */
USTRUCT()
struct HKTINSIGHTS_API FHktInsightEntry
{
    GENERATED_BODY()

    /** 카테고리 (예: "GameMode", "PlayerController", "GridRelevancy") */
    UPROPERTY()
    FString Category;

    /** 키 (예: "CurrentFrame", "RegisteredPlayers") */
    UPROPERTY()
    FString Key;

    /** 값 (문자열로 직렬화) */
    UPROPERTY()
    FString Value;

    /** 심각도: 0=Info, 1=Warning, 2=Error */
    UPROPERTY()
    int32 Severity = 0;

    FHktInsightEntry() = default;

    FHktInsightEntry(const FString& InCategory, const FString& InKey, const FString& InValue, int32 InSeverity = 0)
        : Category(InCategory), Key(InKey), Value(InValue), Severity(InSeverity)
    {}
};

/**
 * FHktInsightSnapshot - 한 Provider가 한 틱에서 수집한 정보 모음
 */
USTRUCT()
struct HKTINSIGHTS_API FHktInsightSnapshot
{
    GENERATED_BODY()

    /** Provider 이름 (클래스명 또는 커스텀 이름) */
    UPROPERTY()
    FString ProviderName;

    /** 수집된 항목들 */
    UPROPERTY()
    TArray<FHktInsightEntry> Entries;

    void Add(const FString& Category, const FString& Key, const FString& Value, int32 Severity = 0)
    {
        Entries.Emplace(Category, Key, Value, Severity);
    }

    void AddInfo(const FString& Category, const FString& Key, const FString& Value)
    {
        Entries.Emplace(Category, Key, Value, 0);
    }

    void AddWarning(const FString& Category, const FString& Key, const FString& Value)
    {
        Entries.Emplace(Category, Key, Value, 1);
    }

    void AddError(const FString& Category, const FString& Key, const FString& Value)
    {
        Entries.Emplace(Category, Key, Value, 2);
    }

    void Reset()
    {
        Entries.Reset();
    }
};

/**
 * IHktInsightProvider - 런타임 인사이트 데이터 제공 인터페이스
 *
 * GameMode, PlayerController, 각 Component 등이 구현합니다.
 * InsightCollector가 매 틱(또는 설정된 간격)마다 CollectInsightData()를 호출하여
 * 현재 상태 정보를 수집합니다.
 *
 * 구현 예시:
 *   void CollectInsightData(FHktInsightSnapshot& OutSnapshot) const override
 *   {
 *       OutSnapshot.ProviderName = TEXT("GameMode");
 *       OutSnapshot.AddInfo(TEXT("Frame"), TEXT("CurrentFrame"), FString::FromInt(CurrentFrame));
 *       OutSnapshot.AddInfo(TEXT("Players"), TEXT("Connected"), FString::FromInt(NumPlayers));
 *   }
 */
UINTERFACE(MinimalAPI)
class UHktInsightProvider : public UInterface
{
    GENERATED_BODY()
};

class HKTINSIGHTS_API IHktInsightProvider
{
    GENERATED_BODY()

public:
    /**
     * 현재 상태 정보를 OutSnapshot에 채워넣습니다.
     * Insight 시스템이 주기적으로 호출합니다.
     *
     * @param OutSnapshot 수집된 데이터를 담을 스냅샷
     */
    virtual void CollectInsightData(FHktInsightSnapshot& OutSnapshot) const = 0;

    /**
     * Provider의 표시 이름 (기본: 클래스 이름)
     * UI에서 탭/섹션 라벨로 사용됩니다.
     */
    virtual FString GetInsightProviderName() const
    {
        return TEXT("Unknown");
    }
};
