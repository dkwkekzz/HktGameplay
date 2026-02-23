// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktInsightsRuntimeTypes.generated.h"

/**
 * EHktPacketDirection - 패킷 방향
 */
UENUM()
enum class EHktPacketDirection : uint8
{
    ClientToServer,  // C2S (Intent RPC 등)
    ServerToClient,  // S2C (FrameBatch, InitialState 등)
};

/**
 * EHktPacketType - 패킷 종류
 */
UENUM()
enum class EHktPacketType : uint8
{
    Intent,            // FHktRuntimeEvent (C2S)
    FrameBatch,        // FHktRuntimeBatch (S2C)
    InitialState,      // FHktRuntimeSimulationState (S2C)
    LoginRequest,      // Server_RequestLogin (C2S)
    LoginResult,       // Client_ReceiveLoginResult (S2C)
    Other,
};

/**
 * FHktPacketRecord - 단일 패킷 기록
 */
USTRUCT()
struct HKTINSIGHTS_API FHktPacketRecord
{
    GENERATED_BODY()

    UPROPERTY()
    double Timestamp = 0.0;

    UPROPERTY()
    EHktPacketDirection Direction = EHktPacketDirection::ClientToServer;

    UPROPERTY()
    EHktPacketType Type = EHktPacketType::Other;

    UPROPERTY()
    int64 PlayerUid = 0;

    /** 프레임 번호 (FrameBatch인 경우) */
    UPROPERTY()
    int64 FrameNumber = 0;

    /** 이벤트 수 (FrameBatch의 Events.Num() 또는 Intent 1개) */
    UPROPERTY()
    int32 EventCount = 0;

    /** 대략적인 직렬화 크기 (바이트) */
    UPROPERTY()
    int32 EstimatedSizeBytes = 0;

    /** 추가 설명 */
    UPROPERTY()
    FString Description;

    FString GetDirectionString() const
    {
        return Direction == EHktPacketDirection::ClientToServer ? TEXT("C→S") : TEXT("S→C");
    }

    FString GetTypeString() const
    {
        switch (Type)
        {
        case EHktPacketType::Intent:       return TEXT("Intent");
        case EHktPacketType::FrameBatch:   return TEXT("FrameBatch");
        case EHktPacketType::InitialState: return TEXT("InitialState");
        case EHktPacketType::LoginRequest: return TEXT("LoginReq");
        case EHktPacketType::LoginResult:  return TEXT("LoginRes");
        default:                           return TEXT("Other");
        }
    }
};

/**
 * FHktWorldEntityRow - 단일 엔티티의 프로퍼티 스냅샷 (디버그용)
 */
USTRUCT()
struct HKTINSIGHTS_API FHktWorldEntityRow
{
    GENERATED_BODY()

    /** 엔티티 ID */
    UPROPERTY()
    int32 EntityId = -1;

    /** 타입 이름 (예: "Unit", "Projectile") */
    UPROPERTY()
    FString TypeName;

    /** LocalIndex 순서의 프로퍼티 이름 목록 */
    UPROPERTY()
    TArray<FString> PropNames;

    /** LocalIndex 순서의 프로퍼티 값 목록 */
    UPROPERTY()
    TArray<int32> PropValues;

    /** 앞 N개 프로퍼티를 "name=val name=val" 형태 요약 문자열로 반환 */
    FString GetPropSummary(int32 MaxProps = 5) const
    {
        FString Out;
        const int32 Count = FMath::Min(PropNames.Num(), MaxProps);
        for (int32 i = 0; i < Count; ++i)
        {
            if (i > 0) Out += TEXT(" ");
            Out += FString::Printf(TEXT("%s=%d"), *PropNames[i], PropValues[i]);
        }
        if (PropNames.Num() > MaxProps)
            Out += FString::Printf(TEXT(" (+%d)"), PropNames.Num() - MaxProps);
        return Out;
    }
};

/**
 * FHktWorldStateSnapshot - 한 시점의 WorldState 전체 스냅샷
 *
 * 서버(Server[0], Server[1] ...) 또는 클라이언트(Client)를 SourceName으로 구분.
 */
USTRUCT()
struct HKTINSIGHTS_API FHktWorldStateSnapshot
{
    GENERATED_BODY()

    /** 소스 이름 (예: "Server[0]", "Client") */
    UPROPERTY()
    FString SourceName;

    /** 이 스냅샷의 시뮬레이션 프레임 번호 */
    UPROPERTY()
    int64 FrameNumber = 0;

    /** 총 활성 엔티티 수 */
    UPROPERTY()
    int32 EntityCount = 0;

    /** 수집 시각 (FPlatformTime::Seconds) */
    UPROPERTY()
    double CaptureTime = 0.0;

    /** 모든 엔티티 행 */
    UPROPERTY()
    TArray<FHktWorldEntityRow> Entities;
};

/**
 * FHktPacketStats - 패킷 통계 (일정 시간 윈도우)
 */
USTRUCT()
struct HKTINSIGHTS_API FHktPacketStats
{
    GENERATED_BODY()

    UPROPERTY()
    int32 TotalPackets = 0;

    UPROPERTY()
    int32 C2S_Count = 0;

    UPROPERTY()
    int32 S2C_Count = 0;

    UPROPERTY()
    int32 IntentCount = 0;

    UPROPERTY()
    int32 FrameBatchCount = 0;

    UPROPERTY()
    int32 InitialStateCount = 0;

    UPROPERTY()
    int64 TotalBytes = 0;

    UPROPERTY()
    float PacketsPerSecond = 0.0f;

    UPROPERTY()
    float BytesPerSecond = 0.0f;
};
