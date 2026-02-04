// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktPersistentTickProvider.h"

/**
 * FHktFilePersistentTickProvider - 파일 기반 영구 프레임 번호 제공자
 *
 * Saved/HktPersistentTick.json에 GlobalFrameCounter를 저장합니다.
 * 개발용/단일 서버 환경에서 사용. 이후 Redis/SQL 등으로 교체 가능.
 */
class HKTRUNTIME_API FHktFilePersistentTickProvider : public IHktPersistentTickProvider
{
public:
    FHktFilePersistentTickProvider();

    virtual void ReserveBatch(int64 BatchSize, TFunction<void(int64 NewMaxFrame)> Callback) override;

private:
    FString GetFilePath() const;
};
