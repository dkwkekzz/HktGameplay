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
// X-매크로 패턴: enum과 이름 문자열을 한 곳에서 관리
// ============================================================================

#define HKT_OPCODE_LIST(X) \
    /* Control Flow */      \
    X(Nop)              \
    X(Halt)             \
    X(Yield)            \
    X(YieldSeconds)     \
    X(Jump)             \
    X(JumpIf)           \
    X(JumpIfNot)        \
    /* Event Wait */        \
    X(WaitCollision)    \
    X(WaitMoveEnd)      \
    /* Data Operations */   \
    X(LoadConst)        \
    X(LoadConstHigh)    \
    X(LoadStore)        \
    X(LoadStoreEntity)  \
    X(SaveStore)        \
    X(SaveStoreEntity)  \
    X(Move)             \
    /* Arithmetic */        \
    X(Add)              \
    X(Sub)              \
    X(Mul)              \
    X(Div)              \
    X(Mod)              \
    X(AddImm)           \
    /* Comparison */        \
    X(CmpEq)            \
    X(CmpNe)            \
    X(CmpLt)            \
    X(CmpLe)            \
    X(CmpGt)            \
    X(CmpGe)            \
    /* Entity */            \
    X(SpawnEntity)      \
    X(DestroyEntity)    \
    /* Spatial Query */     \
    X(GetDistance)       \
    X(FindInRadius)     \
    X(NextFound)        \
    /* Presentation */      \
    X(ApplyEffect)      \
    X(RemoveEffect)     \
    X(PlayVFX)          \
    X(PlayVFXAttached)  \
    X(PlaySound)        \
    X(PlaySoundAtLocation) \
    /* Tags */              \
    X(AddTag)           \
    X(RemoveTag)        \
    X(HasTag)           \
    /* NPC Spawning */      \
    X(CountByTag)       \
    X(GetWorldTime)     \
    X(RandomInt)        \
    X(HasPlayerInGroup) \
    /* Item System */       \
    X(CountByOwner)     \
    X(FindByOwner)      \
    /* Utility */           \
    X(Log)

enum class EOpCode : uint8
{
    #define HKT_OPCODE_ENUM(Name) Name,
    HKT_OPCODE_LIST(HKT_OPCODE_ENUM)
    #undef HKT_OPCODE_ENUM
    Max
};

/** OpCode → 이름 문자열 (디버그/인사이트용) */
inline const TCHAR* GetOpCodeName(EOpCode Op)
{
    static const TCHAR* Names[] = {
        #define HKT_OPCODE_NAME(Name) TEXT(#Name),
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
