#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "EldoriaAnimInstance.generated.h"

class UAnimSequenceBase;

// Worker-thread side: samples the clips captured from the game thread and blends them.
struct FEldoriaAnimProxy : public FAnimInstanceProxy
{
	FEldoriaAnimProxy() = default;
	explicit FEldoriaAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	UAnimSequenceBase* Idle = nullptr;
	UAnimSequenceBase* Move = nullptr;
	UAnimSequenceBase* Action = nullptr;
	UAnimSequenceBase* Walk = nullptr;
	UAnimSequenceBase* Fall = nullptr;
	float WalkTime = 0.f;
	float FallTime = 0.f;
	// 0 = all walk, 1 = all run (only used when there is a walk clip)
	float RunBlend = 1.f;
	float AirWeight = 0.f;
	float IdleTime = 0.f;
	float MoveTime = 0.f;
	float ActionTime = 0.f;
	float MoveWeight = 0.f;
	float ActionWeight = 0.f;
	// a captured pose (mesh bone order, parent space) faded out over the animation
	TArray<FTransform> Frozen;
	float FrozenWeight = 0.f;
	bool bActionUpperBody = false;
};

// Animation for characters driven from code (the Paragon minions, and the hero): idle, walk and run
// blended by speed, a falling loop in the air, and attacks, hit reactions and deaths layered on top.
UCLASS(Transient, NotBlueprintable)
class ELDORIA_API UEldoriaAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void Setup(UAnimSequenceBase* InIdle, UAnimSequenceBase* InMove, float InMoveSpeed);
	// optional: a walk between idle and the run, and a loop while airborne
	void SetupWalkAndFall(UAnimSequenceBase* InWalk, float InWalkSpeed, UAnimSequenceBase* InFall);
	// lets the current action fade out
	void StopAction() { bActionActive = false; }
	// holds the current action where it is (0) or sets it moving again
	void SetActionRate(float Rate) { ActionRate = Rate; }
	// the current action drives only the spine, arms and head; the legs keep walking (a raised guard)
	void SetActionUpperBodyOnly(bool bUpper) { bActionUpperBody = bUpper; }

	// Plays a one-shot clip over the locomotion. With bHold the last frame is kept (deaths).
	// Returns the clip duration in seconds at the given rate.
	// bReverse plays from the last frame back to the first (used to stand up from a knockdown).
	// StartTime >= 0 begins partway through the clip; the returned duration counts from there.
	float PlayAction(UAnimSequenceBase* Anim, float Rate, bool bHold = false, bool bReverse = false, float StartTime = -1.f);

	// Starts from the given pose (one parent-space transform per mesh bone, e.g. where a ragdoll came
	// to rest) and fades it into the animation over Seconds, so there is no snap.
	void BlendFromPose(const TArray<FTransform>& LocalPose, float Seconds);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FEldoriaAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FEldoriaAnimProxy*>(InProxy); }
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	friend struct FEldoriaAnimProxy;

	UPROPERTY() TObjectPtr<UAnimSequenceBase> IdleAnim;
	UPROPERTY() TObjectPtr<UAnimSequenceBase> MoveAnim;
	UPROPERTY() TObjectPtr<UAnimSequenceBase> ActionAnim;
	UPROPERTY() TObjectPtr<UAnimSequenceBase> WalkAnim;
	UPROPERTY() TObjectPtr<UAnimSequenceBase> FallAnim;
	float WalkSpeed = 240.f;
	float WalkTime = 0.f;
	float FallTime = 0.f;
	float RunBlend = 1.f;
	float AirWeight = 0.f;

	float MoveSpeed = 400.f;
	float IdleTime = 0.f;
	float MoveTime = 0.f;
	float ActionTime = 0.f;
	float ActionRate = 1.f;
	float MoveWeight = 0.f;
	float ActionWeight = 0.f;
	TArray<FTransform> FrozenPose;
	float FrozenWeight = 0.f;
	float FrozenBlendTime = 0.5f;
	bool bActionActive = false;
	bool bActionUpperBody = false;
	bool bHold = false;
	bool bReverse = false;
};
