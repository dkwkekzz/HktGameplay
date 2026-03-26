// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ============================================================================
// 레지스터 (StoryBuilder 공개 API)
// ============================================================================

/** 레지스터 인덱스 */
using RegisterIndex = uint8;
constexpr RegisterIndex MaxRegisters = 16;

/**
 * Reg - 특수 레지스터 별칭
 * 
 * R0~R9는 범용 레지스터
 * R10~R15는 특수 목적 레지스터
 */
namespace Reg
{
    // 범용 레지스터
    constexpr RegisterIndex R0 = 0;
    constexpr RegisterIndex R1 = 1;
    constexpr RegisterIndex R2 = 2;
    constexpr RegisterIndex R3 = 3;
    constexpr RegisterIndex R4 = 4;
    constexpr RegisterIndex R5 = 5;
    constexpr RegisterIndex R6 = 6;
    constexpr RegisterIndex R7 = 7;
    constexpr RegisterIndex R8 = 8;
    // R9는 Builder 내부 헬퍼(SaveConst, MoveToward 등)가 Temp로 사용 — 직접 사용 금지
    constexpr RegisterIndex Temp = 9;

    // 특수 목적 레지스터
    constexpr RegisterIndex Self = 10;      // 현재 엔티티 (Event.SourceEntity)
    constexpr RegisterIndex Target = 11;    // 타겟 엔티티 (Event.TargetEntity)
    constexpr RegisterIndex Spawned = 12;   // 최근 생성된 엔티티
    constexpr RegisterIndex Hit = 13;       // 충돌 대상 엔티티
    constexpr RegisterIndex Iter = 14;      // ForEach 순회용 (NextFound 결과)
    constexpr RegisterIndex Flag = 15;      // 범용 플래그/상태
    constexpr RegisterIndex Count = 15;     // 카운트 (Flag와 동일 슬롯)
}

// ============================================================================
// OpCode 정의 (Flow 빌더 / VM 공통)
// X-매크로 패턴: enum, 이름 문자열, 레지스터 사용 정보를 한 곳에서 관리
//
// X(Name, Dst, Src1, Src2) — 각 필드의 역할:
//   _ = 사용 안 함,  R = Read,  W = Write
//
// 주의: Builder 조합 연산(GetPosition, SetPosition, SaveConstEntity 등)은
//       여러 기본 opcode를 emit하므로 개별 opcode 단위로 정의한다.
// ============================================================================

#define HKT_OPCODE_LIST(X) \
    /* Control Flow */      \
    X(Nop,              _, _, _) \
    X(Halt,             _, _, _) \
    X(Fail,             _, _, _) \
    X(Yield,            _, _, _) \
    X(YieldSeconds,     _, _, _) \
    X(Jump,             _, _, _) \
    X(JumpIf,           _, R, _) \
    X(JumpIfNot,        _, R, _) \
    /* Event Wait */        \
    X(WaitCollision,    _, R, _) \
    X(WaitMoveEnd,      _, R, _) \
    /* Data Operations */   \
    X(LoadConst,        W, _, _) \
    X(LoadConstHigh,    W, _, _) \
    X(LoadStore,        W, _, _) \
    X(LoadStoreEntity,  W, R, _) \
    X(SaveStore,        _, R, _) \
    X(SaveStoreEntity,  _, R, R) \
    X(Move,             W, R, _) \
    /* Arithmetic */        \
    X(Add,              W, R, R) \
    X(Sub,              W, R, R) \
    X(Mul,              W, R, R) \
    X(Div,              W, R, R) \
    X(Mod,              W, R, R) \
    X(AddImm,           W, R, _) \
    /* Comparison */        \
    X(CmpEq,            W, R, R) \
    X(CmpNe,            W, R, R) \
    X(CmpLt,            W, R, R) \
    X(CmpLe,            W, R, R) \
    X(CmpGt,            W, R, R) \
    X(CmpGe,            W, R, R) \
    /* Entity */            \
    X(SpawnEntity,      _, _, _) \
    X(DestroyEntity,    _, R, _) \
    /* Spatial Query */     \
    X(GetDistance,       W, R, R) \
    X(LookAt,           _, R, R) \
    X(FindInRadius,     _, R, _) \
    X(NextFound,        _, _, _) \
    /* Presentation */      \
    X(ApplyEffect,      _, R, _) \
    X(RemoveEffect,     _, R, _) \
    X(PlayVFX,          _, R, _) \
    X(PlayVFXAttached,  _, R, _) \
    X(PlaySound,        _, _, _) \
    X(PlaySoundAtLocation, _, R, _) \
    /* Tags */              \
    X(AddTag,           _, R, _) \
    X(RemoveTag,        _, R, _) \
    X(HasTag,           W, R, _) \
    /* NPC Spawning */      \
    X(CountByTag,       W, _, _) \
    X(GetWorldTime,     W, _, _) \
    X(RandomInt,        W, R, _) \
    X(HasPlayerInGroup, W, _, _) \
    /* Item System */       \
    X(CountByOwner,     W, R, _) \
    X(FindByOwner,      _, R, _) \
    X(SetOwnerUid,      _, R, _) \
    X(ClearOwnerUid,    _, R, _) \
    /* Event Dispatch */    \
    X(DispatchEvent,    _, _, _) \
    X(DispatchEventTo,  _, R, _) \
    /* Utility */           \
    X(Log,              _, _, _)

enum class EOpCode : uint8
{
    #define HKT_OPCODE_ENUM(Name, ...) Name,
    HKT_OPCODE_LIST(HKT_OPCODE_ENUM)
    #undef HKT_OPCODE_ENUM
    Max
};

/** OpCode → 이름 문자열 (디버그/인사이트용) */
inline const TCHAR* GetOpCodeName(EOpCode Op)
{
    static const TCHAR* Names[] = {
        #define HKT_OPCODE_NAME(Name, ...) TEXT(#Name),
        HKT_OPCODE_LIST(HKT_OPCODE_NAME)
        #undef HKT_OPCODE_NAME
    };
    const uint8 Index = static_cast<uint8>(Op);
    return Index < static_cast<uint8>(EOpCode::Max) ? Names[Index] : TEXT("Unknown");
}

// ============================================================================
// 명령어 인코딩 (Flow 빌더 / VM 공통)
// ============================================================================

/**
 * 32비트 명령어 포맷:
 * [OpCode:8][Dst:4][Src1:4][Src2:4][Imm12:12] - 3-operand
 * [OpCode:8][Dst:4][Imm20:20]                 - Load immediate
 */
struct FInstruction
{
    union
    {
        uint32 Raw;
        struct
        {
            uint32 OpCode : 8;
            uint32 Dst : 4;
            uint32 Src1 : 4;
            uint32 Src2 : 4;
            uint32 Imm12 : 12;
        };
        struct
        {
            uint32 _Op : 8;
            uint32 _Dst : 4;
            uint32 Imm20 : 20;
        };
    };

    FInstruction() : Raw(0) {}
    explicit FInstruction(uint32 InRaw) : Raw(InRaw) {}

    EOpCode GetOpCode() const { return static_cast<EOpCode>(OpCode); }

    // 빌더 헬퍼
    static FInstruction Make(EOpCode Op, uint8 InDst = 0, uint8 InSrc1 = 0, uint8 InSrc2 = 0, uint16 Imm = 0)
    {
        FInstruction I;
        I.OpCode = static_cast<uint8>(Op);
        I.Dst = InDst;
        I.Src1 = InSrc1;
        I.Src2 = InSrc2;
        I.Imm12 = Imm;
        return I;
    }

    static FInstruction MakeImm(EOpCode Op, uint8 InDst, int32 Imm)
    {
        FInstruction I;
        I.OpCode = static_cast<uint8>(Op);
        I._Dst = InDst;
        I.Imm20 = static_cast<uint32>(Imm) & 0xFFFFF;
        return I;
    }

    int32 GetSignedImm12() const
    {
        // 12-bit sign extension
        int32 Val = Imm12;
        if (Val & 0x800)
        {
            Val |= 0xFFFFF000;
        }
        return Val;
    }

    int32 GetSignedImm20() const
    {
        // 20-bit sign extension
        int32 Val = Imm20;
        if (Val & 0x80000)
        {
            Val |= 0xFFF00000;
        }
        return Val;
    }
};

static_assert(sizeof(FInstruction) == 4, "Instruction must be 32 bits");

// ============================================================================
// OpCode 레지스터 사용 정보 (Build-time 검증용)
// ============================================================================

/** 레지스터 필드의 역할 */
enum class ERegRole : uint8 { None, Read, Write };

/** OpCode별 Dst/Src1/Src2 레지스터 역할 */
struct FOpRegInfo
{
    ERegRole Dst;
    ERegRole Src1;
    ERegRole Src2;
};

/** OpCode → 레지스터 사용 정보 조회 (X-매크로에서 자동 생성) */
inline FOpRegInfo GetOpRegInfo(EOpCode Op)
{
    // X-매크로의 _, R, W 토큰을 ERegRole로 변환
    #define HKT_REG_ROLE__ ERegRole::None
    #define HKT_REG_ROLE_R ERegRole::Read
    #define HKT_REG_ROLE_W ERegRole::Write
    #define HKT_REG_ROLE(Token) HKT_REG_ROLE_##Token

    static const FOpRegInfo Table[] = {
        #define HKT_OPCODE_REG(Name, D, S1, S2) { HKT_REG_ROLE(D), HKT_REG_ROLE(S1), HKT_REG_ROLE(S2) },
        HKT_OPCODE_LIST(HKT_OPCODE_REG)
        #undef HKT_OPCODE_REG
    };

    #undef HKT_REG_ROLE__
    #undef HKT_REG_ROLE_R
    #undef HKT_REG_ROLE_W
    #undef HKT_REG_ROLE

    const uint8 Index = static_cast<uint8>(Op);
    if (Index < static_cast<uint8>(EOpCode::Max))
        return Table[Index];
    return { ERegRole::None, ERegRole::None, ERegRole::None };
}
