// Copyright Hkt Studios, Inc. All Rights Reserved.

// 런타임: 시뮬레이션 이벤트 → 미리 생성된 Niagara 시스템 스폰 + 파라미터 오버라이드

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "VFXIntent.h"
#include "VFXRuntimeResolver.generated.h"

// ============================================================================
// VFX 에셋 뱅크 - 에디터에서 생성된 에셋 참조 테이블
// ============================================================================
USTRUCT(BlueprintType)
struct FVFXAssetEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Key;  // Intent.GetAssetKey() 결과

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UNiagaraSystem> System;
};

UCLASS(BlueprintType)
class HKTVFX_API UVFXAssetBank : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FVFXAssetEntry> Entries;

    // 키로 시스템 검색
    UNiagaraSystem* FindSystem(const FString& Key) const
    {
        for (const auto& Entry : Entries)
        {
            if (Entry.Key == Key)
            {
                return Entry.System.LoadSynchronous();
            }
        }
        return nullptr;
    }

    // 가장 유사한 키 검색 (정확한 매칭 실패 시)
    UNiagaraSystem* FindClosestSystem(const FVFXIntent& Intent) const
    {
        // 1. 정확한 매칭
        FString ExactKey = Intent.GetAssetKey();
        if (UNiagaraSystem* Exact = FindSystem(ExactKey))
            return Exact;

        // 2. Intensity 무시하고 매칭
        FString BaseKey = FString::Printf(TEXT("VFX_%s_%s"),
            *UEnum::GetValueAsString(Intent.EventType).RightChop(16),
            *UEnum::GetValueAsString(Intent.Element).RightChop(14));

        UNiagaraSystem* BestMatch = nullptr;
        float BestIntensityDiff = MAX_FLT;

        for (const auto& Entry : Entries)
        {
            if (Entry.Key.StartsWith(BaseKey))
            {
                // 키에서 intensity 추출하여 가장 가까운 것 선택
                // Key 형식: VFX_EventType_Element_I{N}[_Surface]
                int32 IPos = Entry.Key.Find(TEXT("_I"));
                if (IPos != INDEX_NONE)
                {
                    FString IntStr = Entry.Key.Mid(IPos + 2, 1);
                    float EntryIntensity = FCString::Atof(*IntStr) / 10.f;
                    float Diff = FMath::Abs(EntryIntensity - Intent.Intensity);
                    if (Diff < BestIntensityDiff)
                    {
                        BestIntensityDiff = Diff;
                        BestMatch = Entry.System.LoadSynchronous();
                    }
                }
            }
        }

        return BestMatch;
    }
};

// ============================================================================
// UVFXRuntimeResolver - 게임플레이 코드에서 사용하는 컴포넌트
// ============================================================================
UCLASS(ClassGroup=(VFX), meta=(BlueprintSpawnableComponent))
class HKTVFX_API UVFXRuntimeResolver : public UActorComponent
{
    GENERATED_BODY()

public:
    // 에셋 뱅크 (에디터에서 할당)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    UVFXAssetBank* AssetBank;

    // 폴백 시스템 (매칭 실패 시)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    UNiagaraSystem* FallbackSystem;

    // === 메인 API: 시뮬레이션에서 호출 ===

    UFUNCTION(BlueprintCallable, Category="VFX")
    UNiagaraComponent* PlayVFX(const FVFXIntent& Intent)
    {
        if (!AssetBank)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VFXResolver] No AssetBank assigned"));
            return nullptr;
        }

        // 1. 에셋 뱅크에서 시스템 검색
        UNiagaraSystem* System = AssetBank->FindClosestSystem(Intent);
        if (!System)
        {
            System = FallbackSystem;
            if (!System)
            {
                UE_LOG(LogTemp, Warning, TEXT("[VFXResolver] No matching VFX for: %s"),
                    *Intent.GetAssetKey());
                return nullptr;
            }
        }

        // 2. 스폰
        UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(),
            System,
            Intent.Location,
            Intent.Direction.Rotation(),
            FVector::OneVector,
            true,   // bAutoDestroy
            true,   // bAutoActivate
            ENCPoolMethod::AutoRelease);

        if (!Comp) return nullptr;

        // 3. 런타임 파라미터 오버라이드
        ApplyRuntimeOverrides(Comp, Intent);

        return Comp;
    }

    // Attached 버전 (캐릭터에 붙는 VFX용)
    UFUNCTION(BlueprintCallable, Category="VFX")
    UNiagaraComponent* PlayVFXAttached(const FVFXIntent& Intent, USceneComponent* AttachTo,
        FName SocketName = NAME_None)
    {
        if (!AssetBank || !AttachTo) return nullptr;

        UNiagaraSystem* System = AssetBank->FindClosestSystem(Intent);
        if (!System) System = FallbackSystem;
        if (!System) return nullptr;

        UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
            System,
            AttachTo,
            SocketName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            true,   // bAutoDestroy
            true,   // bAutoActivate
            ENCPoolMethod::AutoRelease);

        if (Comp)
        {
            ApplyRuntimeOverrides(Comp, Intent);
        }
        return Comp;
    }

private:
    void ApplyRuntimeOverrides(UNiagaraComponent* Comp, const FVFXIntent& Intent)
    {
        // 크기 스케일
        float RadiusScale = Intent.Radius / 200.f;  // 기본 200 유닛 기준
        Comp->SetVariableFloat(FName("RadiusScale"), RadiusScale);

        // 강도
        Comp->SetVariableFloat(FName("IntensityMult"), Intent.Intensity);

        // 지속시간
        if (Intent.Duration > 0.f)
        {
            Comp->SetVariableFloat(FName("DurationScale"), Intent.Duration);
        }

        // 파워 레벨 (이펙트 화려함)
        Comp->SetVariableFloat(FName("PowerLevel"), Intent.SourcePower);

        // 속성 기반 색상 틴트 (미세 조정)
        FLinearColor ElementTint = GetElementTintColor(Intent.Element);
        Comp->SetVariableLinearColor(FName("ElementTint"), ElementTint);

        // 전체 스케일
        float OverallScale = FMath::Lerp(0.5f, 2.0f, Intent.Intensity);
        Comp->SetWorldScale3D(FVector(OverallScale * RadiusScale));
    }

    static FLinearColor GetElementTintColor(EVFXElement Element)
    {
        switch (Element)
        {
        case EVFXElement::Fire:      return FLinearColor(1.0f, 0.6f, 0.2f, 1.0f);
        case EVFXElement::Ice:       return FLinearColor(0.5f, 0.8f, 1.0f, 1.0f);
        case EVFXElement::Lightning: return FLinearColor(0.7f, 0.9f, 1.0f, 1.0f);
        case EVFXElement::Water:     return FLinearColor(0.3f, 0.5f, 0.9f, 1.0f);
        case EVFXElement::Earth:     return FLinearColor(0.6f, 0.4f, 0.2f, 1.0f);
        case EVFXElement::Wind:      return FLinearColor(0.8f, 0.9f, 0.8f, 1.0f);
        case EVFXElement::Dark:      return FLinearColor(0.4f, 0.1f, 0.6f, 1.0f);
        case EVFXElement::Holy:      return FLinearColor(1.0f, 0.95f, 0.7f, 1.0f);
        case EVFXElement::Poison:    return FLinearColor(0.3f, 0.8f, 0.2f, 1.0f);
        case EVFXElement::Arcane:    return FLinearColor(0.5f, 0.3f, 0.9f, 1.0f);
        case EVFXElement::Nature:    return FLinearColor(0.4f, 0.8f, 0.3f, 1.0f);
        default:                     return FLinearColor::White;
        }
    }
};