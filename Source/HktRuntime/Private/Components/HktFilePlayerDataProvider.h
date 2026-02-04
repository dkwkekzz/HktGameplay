// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktPlayerDataProvider.h"

/**
 * FHktFilePlayerDataProvider - 파일 기반 플레이어 데이터 제공자
 *
 * Saved/HktPlayerDatabase/{PlayerId}.json 형태로 플레이어별 파일 저장.
 * 개발용/단일 서버 환경에서 사용. 이후 Redis/SQL 등으로 교체 가능.
 */
class HKTRUNTIME_API FHktFilePlayerDataProvider : public IHktPlayerDataProvider
{
public:
    FHktFilePlayerDataProvider();

    virtual void Load(const FString& PlayerId, TFunction<void(TOptional<FHktPlayerRecord>)> Callback) override;
    virtual void Save(const FString& PlayerId, const FHktPlayerRecord& Record, TFunction<void(bool bSuccess)> Callback) override;

private:
    FString GetFilePath(const FString& PlayerId) const;
    static FString SanitizePlayerIdForPath(const FString& PlayerId);
};
