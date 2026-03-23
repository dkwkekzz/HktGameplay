// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Snippets/HktSnippetCombat.h"
#include "HktCoreProperties.h"

FHktStoryBuilder& HktSnippetCombat::CooldownCheck(
	FHktStoryBuilder& B,
	const FString& FailLabel)
{
	using namespace Reg;

	B.GetWorldTime(R0)                                       // R0 = 현재 프레임
	 .LoadStore(R1, PropertyId::NextActionFrame)             // R1 = NextActionFrame
	 .CmpLt(Flag, R0, R1)                                    // 현재 < NextActionFrame?
	 .JumpIf(Flag, FailLabel);                               // 아직 쿨타임 중이면 실패

	return B;
}

FHktStoryBuilder& HktSnippetCombat::CooldownUpdateConst(
	FHktStoryBuilder& B,
	int32 RecoveryFrame)
{
	using namespace Reg;

	// NextActionFrame = 현재프레임 + (RecoveryFrame * 100 / AttackSpeed)
	B.GetWorldTime(R0)                                       // R0 = 현재 프레임
	 .LoadConst(R1, RecoveryFrame)                           // R1 = 기본 후딜레이
	 .LoadConst(R2, 100)                                     // R2 = 100 (스케일 상수)
	 .Mul(R1, R1, R2)                                        // R1 = RecoveryFrame * 100
	 .LoadStore(R2, PropertyId::AttackSpeed)                 // R2 = AttackSpeed
	 .Div(R1, R1, R2)                                        // R1 = RecoveryFrame * 100 / AttackSpeed
	 .Add(R0, R0, R1)                                        // R0 = 현재프레임 + 딜레이
	 .SaveStore(PropertyId::NextActionFrame, R0);            // NextActionFrame 저장

	return B;
}

FHktStoryBuilder& HktSnippetCombat::CooldownUpdateFromEntity(
	FHktStoryBuilder& B,
	RegisterIndex ItemEntity)
{
	using namespace Reg;

	// NextActionFrame = 현재프레임 + (Item.RecoveryFrame * 100 / AttackSpeed)
	B.GetWorldTime(R0)                                                   // R0 = 현재 프레임
	 .LoadEntityProperty(R1, ItemEntity, PropertyId::RecoveryFrame)      // R1 = 아이템의 기본 후딜레이
	 .LoadConst(R3, 100)
	 .Mul(R1, R1, R3)                                                    // R1 = RecoveryFrame * 100
	 .LoadStore(R3, PropertyId::AttackSpeed)                             // R3 = AttackSpeed
	 .Div(R1, R1, R3)                                                    // R1 = delay (프레임 수)
	 .Add(R0, R0, R1)                                                    // R0 = 현재프레임 + delay
	 .SaveStore(PropertyId::NextActionFrame, R0);

	return B;
}

FHktStoryBuilder& HktSnippetCombat::ResourceGainClamped(
	FHktStoryBuilder& B,
	uint16 CurrentProp,
	uint16 MaxProp,
	int32 Amount)
{
	using namespace Reg;

	FString NoClampLabel = B.MakeInternalLabel(TEXT("noclamp"));

	B.LoadStore(R0, CurrentProp)                             // R0 = 현재 값
	 .LoadStore(R1, MaxProp)                                 // R1 = 최대 값
	 .LoadConst(R2, Amount)                                  // R2 = 회복량
	 .Add(R0, R0, R2)                                        // R0 = 현재 + 회복량
	 .CmpGt(R3, R0, R1)                                      // 초과?
	 .JumpIfNot(R3, NoClampLabel)
	 .Move(R0, R1)                                           // Max로 제한
	 .Label(NoClampLabel)
	 .SaveStore(CurrentProp, R0);                            // 저장

	return B;
}
