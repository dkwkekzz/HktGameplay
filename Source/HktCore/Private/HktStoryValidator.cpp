// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryValidator.h"
#include "HktCoreLog.h"
#include "HktCoreEventLog.h"

FHktStoryValidator::FHktStoryValidator(
	const TArray<FInstruction>& InCode,
	const FGameplayTag& InTag,
	const TMap<FString, int32>& InLabels)
	: Code(InCode)
	, Tag(InTag)
{
	for (const auto& Pair : InLabels)
	{
		LabelPCs.Add(Pair.Value);
	}
}

// ============================================================================
// Entity Register Validation (R10~R14)
// ============================================================================

bool FHktStoryValidator::ValidateEntityFlow()
{
	bool bValid = true;
	// Self(R10), Target(R11)은 이벤트에서 항상 초기화됨
	// Spawned(R12), Hit(R13), Iter(R14)는 특정 Op 실행 후에만 유효
	uint16 EntityRegs = (1 << Reg::Self) | (1 << Reg::Target);

	auto GetEntityRegName = [](RegisterIndex R) -> const TCHAR*
	{
		switch (R)
		{
		case Reg::Self:    return TEXT("Self");
		case Reg::Target:  return TEXT("Target");
		case Reg::Spawned: return TEXT("Spawned");
		case Reg::Hit:     return TEXT("Hit");
		case Reg::Iter:    return TEXT("Iter");
		default:           return nullptr;
		}
	};

	// 특수 엔티티 레지스터(R10~R14)가 초기화되기 전에 사용되는지 검사
	auto CheckEntityReg = [&](int32 PC, EOpCode Op, RegisterIndex R)
	{
		const TCHAR* Name = GetEntityRegName(R);
		if (!Name)
			return;

		if (!(EntityRegs & (1 << R)))
		{
			HKT_EVENT_LOG(HktLogTags::Core_Story, EHktLogLevel::Error, EHktLogSource::Server, FString::Printf(
				TEXT("Story BUILD: %s PC=%d Op=%s — Reg %s (R%d) 가 엔티티로 사용되었지만 이전에 초기화되지 않았습니다. "
					 "SpawnEntity/WaitCollision/NextFound 호출 순서를 확인하세요."),
				*Tag.ToString(), PC, GetOpCodeName(Op), Name, R));
			bValid = false;
		}
	};

	for (int32 PC = 0; PC < Code.Num(); ++PC)
	{
		const FInstruction& Inst = Code[PC];
		EOpCode Op = Inst.GetOpCode();

		switch (Op)
		{
		// --- Entity register writers ---
		case EOpCode::SpawnEntity:
			EntityRegs |= (1 << Reg::Spawned);
			break;
		case EOpCode::WaitCollision:
			CheckEntityReg(PC, Op, Inst.Src1);
			EntityRegs |= (1 << Reg::Hit);
			break;
		case EOpCode::NextFound:
			EntityRegs |= (1 << Reg::Iter);
			break;

		// --- Entity register readers (Src1 = entity) ---
		case EOpCode::LoadStoreEntity:
		case EOpCode::SaveStoreEntity:
		case EOpCode::DestroyEntity:
		case EOpCode::FindInRadius:
			CheckEntityReg(PC, Op, Inst.Src1);
			break;
		case EOpCode::GetDistance:
		case EOpCode::LookAt:
			CheckEntityReg(PC, Op, Inst.Src1);
			CheckEntityReg(PC, Op, Inst.Src2);
			break;
		case EOpCode::AddTag:
		case EOpCode::RemoveTag:
		case EOpCode::HasTag:
		case EOpCode::PlayVFXAttached:
		case EOpCode::ApplyEffect:
		case EOpCode::RemoveEffect:
		case EOpCode::SetOwnerUid:
		case EOpCode::ClearOwnerUid:
		case EOpCode::CountByOwner:
		case EOpCode::FindByOwner:
		case EOpCode::WaitMoveEnd:
		case EOpCode::WaitAnimEnd:
			CheckEntityReg(PC, Op, Inst.Src1);
			break;

		default:
			break;
		}
	}

	return bValid;
}

// ============================================================================
// General Register Flow Validation (R0~R8)
// ============================================================================

int32 FHktStoryValidator::ValidateRegisterFlow()
{
	/**
	 * 범용 레지스터(R0~R8) 흐름을 선형 스캔하여 두 가지 패턴을 감지:
	 *
	 * 1. Read-before-Write: 초기화 안 된 레지스터를 읽는 경우
	 *    → 이전 Story 실행의 잔류값에 의존하는 잠재 버그
	 *
	 * 2. Dead Write (Write-Write-without-Read): 값을 쓰고 읽지 않고 다시 덮어쓰는 경우
	 *    → Snippet 파라미터 레지스터가 내부 temp에 의해 덮어씌워지는 유형의 버그
	 *
	 * Label(합류점)에서는 상태를 보수적으로 리셋하여 오탐을 방지한다.
	 */
	constexpr int32 NumGPRegs = 9;  // R0~R8
	int32 WarningCount = 0;

	enum class ERegState : uint8 { Unknown, Written, Read };
	ERegState State[NumGPRegs];
	int32 WritePC[NumGPRegs];
	for (int32 i = 0; i < NumGPRegs; ++i)
	{
		State[i] = ERegState::Unknown;
		WritePC[i] = -1;
	}

	auto MarkRead = [&](int32 PC, EOpCode Op, RegisterIndex R)
	{
		if (R >= NumGPRegs) return;
		if (State[R] == ERegState::Unknown)
		{
			HKT_EVENT_LOG(HktLogTags::Core_Story, EHktLogLevel::Warning, EHktLogSource::Server, FString::Printf(
				TEXT("Story REGFLOW: %s PC=%d Op=%s — R%d Read-before-Write. "
					 "초기화되지 않은 레지스터를 읽고 있습니다."),
				*Tag.ToString(), PC, GetOpCodeName(Op), R));
			++WarningCount;
		}
		State[R] = ERegState::Read;
	};

	auto MarkWrite = [&](int32 PC, EOpCode Op, RegisterIndex R)
	{
		if (R >= NumGPRegs) return;
		if (State[R] == ERegState::Written)
		{
			HKT_EVENT_LOG(HktLogTags::Core_Story, EHktLogLevel::Warning, EHktLogSource::Server, FString::Printf(
				TEXT("Story REGFLOW: %s PC=%d Op=%s — R%d Dead Write. "
					 "PC=%d에서 쓴 값을 읽지 않고 덮어쓰고 있습니다. 레지스터 충돌을 확인하세요."),
				*Tag.ToString(), PC, GetOpCodeName(Op), R, WritePC[R]));
			++WarningCount;
		}
		State[R] = ERegState::Written;
		WritePC[R] = PC;
	};

	for (int32 PC = 0; PC < Code.Num(); ++PC)
	{
		// Label 합류점: 다른 경로에서 올 수 있으므로 상태를 보수적으로 Read로 리셋
		if (LabelPCs.Contains(PC))
		{
			for (int32 i = 0; i < NumGPRegs; ++i)
			{
				State[i] = ERegState::Read;
			}
		}

		const FInstruction& Inst = Code[PC];
		EOpCode Op = Inst.GetOpCode();
		FOpRegInfo Info = GetOpRegInfo(Op);

		// Read를 먼저 처리 (같은 명령에서 Read+Write면 Read가 먼저 발생)
		if (Info.Src1 == ERegRole::Read)
			MarkRead(PC, Op, Inst.Src1);
		if (Info.Src2 == ERegRole::Read)
			MarkRead(PC, Op, Inst.Src2);

		// Write 처리
		if (Info.Dst == ERegRole::Write)
			MarkWrite(PC, Op, Inst.Dst);
	}

	return WarningCount;
}
