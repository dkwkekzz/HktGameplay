// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAssetSettings.h"

UHktAssetSettings::UHktAssetSettings()
{
	// 기본 Convention 규칙 — 프로젝트 설정에서 변경 가능
	ConventionRules = {
		// Entity.Character.{Name} → {Root}/Characters/{Name}/BP_{Name}
		{ TEXT("Entity.Character."), TEXT("{Root}/Characters/{Leaf}/BP_{Leaf}") },

		// Entity.{Type}.{Name} → {Root}/Entities/{Name}/BP_{Name}
		{ TEXT("Entity."), TEXT("{Root}/Entities/{Leaf}/BP_{Leaf}") },

		// VFX.{...} → {Root}/VFX/{TagPath}
		{ TEXT("VFX."), TEXT("{Root}/VFX/{TagPath}") },

		// Equipment.{Category}.{SubType} → {Root}/Items/{Category}/SM_{Leaf}
		{ TEXT("Equipment."), TEXT("{Root}/Items/{Category}/SM_{Leaf}") },

		// Anim.{...} → {Root}/Animations/{TagPath}
		{ TEXT("Anim."), TEXT("{Root}/Animations/{TagPath}") },
	};
}

FString UHktAssetSettings::ResolveConventionPath(const FString& TagStr) const
{
	if (TagStr.IsEmpty()) return FString();

	// 규칙 매칭 (순서대로)
	const FHktConventionRule* MatchedRule = nullptr;
	for (const FHktConventionRule& Rule : ConventionRules)
	{
		if (TagStr.StartsWith(Rule.TagPrefix))
		{
			MatchedRule = &Rule;
			break;
		}
	}

	if (!MatchedRule) return FString();

	// 태그 세그먼트 파싱
	TArray<FString> Parts;
	TagStr.ParseIntoArray(Parts, TEXT("."));

	// {Leaf} — 마지막 세그먼트
	FString Leaf = Parts.Num() > 0 ? Parts.Last() : TagStr;

	// {Category} — 두 번째 세그먼트 (없으면 빈 문자열)
	FString Category = Parts.Num() >= 2 ? Parts[1] : TEXT("");

	// {TagPath} — 전체를 _ 로 연결
	FString TagPath = TagStr;
	TagPath.ReplaceInline(TEXT("."), TEXT("_"));

	// 패턴 치환
	FString Result = MatchedRule->PathPattern;
	Result.ReplaceInline(TEXT("{Root}"), *ConventionRootDirectory);
	Result.ReplaceInline(TEXT("{Leaf}"), *Leaf);
	Result.ReplaceInline(TEXT("{Category}"), *Category);
	Result.ReplaceInline(TEXT("{TagPath}"), *TagPath);

	return Result;
}
