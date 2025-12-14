#pragma once

#include "CoreMinimal.h"
#include "SKillBase.h"
#include "BuffSkill.generated.h"


UCLASS(Blueprintable, BlueprintType)
class PROJECTD_API ABuffSkill : public ASKillBase
{
	GENERATED_BODY()

public:
	ABuffSkill();

#pragma region EngineCallbackFunction
public:
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray< class FLifetimeProperty >& OutLifetimeProps) const override;
protected:
	virtual void BeginPlay() override;
#pragma endregion EngineCallbackFunction

#pragma region Variable
protected:
	// 1번 스킬 타이머
	FTimerHandle BuffDurationHandle_Overdrive;

	// 2번 스킬 타이머
	FTimerHandle BuffDurationHandle_XrayVision;

	// 투시 스킬을 위한 머티리얼 변수
	UPROPERTY(Replicated)
	TObjectPtr<UMaterialInterface> XrayOcclusionMaterial;

	UPROPERTY(Replicated)
	TObjectPtr<UMaterialInterface> XrayOutlineMaterial;

#pragma endregion Variable

#pragma region CustomFuction
public:
	// 스킬 동작 구현 (재정의)
	virtual void UseSkill_Implementation(const FName& _SkillName) override;

	void EndBuff_Overdrive();

	void EndBuff_XrayVision();

	// 서버에서 클라이언트 카메라 제어 못함
	UFUNCTION(Client, Reliable)
	void Client_SetPostProcessWeight(float _Weight);
	void Client_SetPostProcessWeight_Implementation(float _Weight);

	virtual void InitXrayMaterial(UCameraComponent* _Camera) override;

	// 인게임에 모든 생존자가 소환되었는지 확인하고 생존자 캐릭터 저장
	UFUNCTION(BlueprintCallable)
	void InitSurvivorMesh();

	bool bInitMeshTrigger = true;

	// XRayVision 스킬인 경우 월드에 존재하는 모든 생존자 액터를 받아두는 용도
	UPROPERTY()
	TArray<AActor*> mSurvivors;

	// 투시 스킬에 사용될 생존자 메쉬 정보
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> mSurvivorsMeshes;

	// 카메라에 포스트 프로세스 적용하기 위해 카메라 정보 저장
	TObjectPtr<UCameraComponent> mOwnerCameraCached;

#pragma endregion CustomFuction

#pragma region GetSet 
public:
	// 사망한 생존자의 메쉬 정보 갱신
	virtual void Client_SetSurvivorMeshUpdate_Implementation(AActor* _Survivor) override;

#pragma endregion GetSet 
};
