// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Styling/SlateBrush.h"

class SEditableTextBox;
class UHktWidgetLoginHudDataAsset;

DECLARE_DELEGATE_TwoParams(FOnHktLoginRequested, const FString&, const FString&);

/**
 * 로그인 화면 Slate 위젯.
 * ID/비밀번호 입력, 로그인 버튼. 배경 Brush는 인자로 전달 가능.
 */
class HKTPRESENTATION_API SHktLoginHudWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktLoginHudWidget) {}
		/** 로그인 버튼 클릭 시 (ID, PW) */
		SLATE_EVENT(FOnHktLoginRequested, OnLoginRequested)
		/** 배경 Brush (일러스트 등). 없으면 비움 */
		SLATE_ARGUMENT(TOptional<FSlateBrush>, BackgroundBrush)
		/** 로그인 위젯용 데이터 에셋 (미디어 등 이미 로드된 상태로 전달) */
		SLATE_ARGUMENT(UHktWidgetLoginHudDataAsset*, LoginWidgetDataAsset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnLoginClicked();

	/** 데이터 에셋에서 미디어를 꺼내 동영상 배경 적용 */
	void ApplyVideoFromDataAsset(UHktWidgetLoginHudDataAsset* DataAsset);

	TSharedPtr<SEditableTextBox> IdTextBox;
	TSharedPtr<SEditableTextBox> PasswordTextBox;
	FOnHktLoginRequested OnLoginRequested;
	/** 배경이 있으면 유효한 Brush 보관 (SImage 포인터 유지용) */
	TOptional<FSlateBrush> CachedBackgroundBrush;

	/** 동영상 배경용 Slate 브러시 (생명주기 동안 유효해야 하므로 멤버 변수) */
	FSlateBrush VideoBrush;
	/** 가비지 컬렉션 방지를 위한 강한 참조 */
	TStrongObjectPtr<UMediaPlayer> MediaPlayer;
	TStrongObjectPtr<UMediaTexture> MediaTexture;
	TStrongObjectPtr<UFileMediaSource> MediaSource;
};
