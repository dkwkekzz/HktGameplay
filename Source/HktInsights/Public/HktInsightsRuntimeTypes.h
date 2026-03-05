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
 * FHktEntityListEntry - 엔티티 리스트 1행 (이벤트 기반 갱신)
 */
struct HKTINSIGHTS_API FHktEntityListEntry
{
    FString Source;
    int32 EntityId = -1;
    FString TypeName;
};

/**
 * FHktEntitySelection - 선택된 엔티티 식별자 (Panel ↔ Provider 통신용)
 */
struct HKTINSIGHTS_API FHktEntitySelection
{
    FString Source;
    int32 EntityId = -1;

    bool IsValid() const { return EntityId >= 0 && !Source.IsEmpty(); }
    void Reset() { Source.Reset(); EntityId = -1; }
};

/**
 * FHktVMStateInfo - 엔티티에 바인딩된 VM 실행 상태
 */
struct HKTINSIGHTS_API FHktVMStateInfo
{
    int32 PC = 0;
    FString StatusName;
    FString ProgramTag;
    int64 PlayerUid = 0;
    int32 CreationFrame = 0;
    int32 SelfEntity = -1;
    int32 TargetEntity = -1;
    int32 SourceEventId = 0;
};

/**
 * FHktSelectedEntityDetail - 선택된 엔티티의 전체 상세 정보
 */
struct HKTINSIGHTS_API FHktSelectedEntityDetail
{
    int32 EntityId = -1;
    FString Source;
    int64 FrameNumber = 0;
    TArray<FString> PropNames;
    TArray<int32> PropValues;
    int64 OwnerUid = 0;

    bool IsValid() const { return EntityId >= 0; }
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
