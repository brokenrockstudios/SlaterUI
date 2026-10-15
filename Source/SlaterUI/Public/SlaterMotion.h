// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveHandle.h"

namespace Slater
{
	/** The engine ease for a word of `Slater::EaseWords()`; false for any other word. */
	SLATERUI_API bool EaseFromWord(FName Word, ECurveEaseFunction& OutEase);

	/** `Slater.MotionScale`: multiplies every transition's duration. 0 makes every transition instant (tests, a reduce-motion setting). */
	SLATERUI_API float GetMotionScale();
}

/**
 * One number moving toward a target over a duration with an ease (T-406). Pure: no Slate, no clock of its own; the caller
 * feeds it time with `Advance`, so a test can drive it with a manual clock. `Start` always begins at the current value,
 * so a retarget in mid-flight continues from where the number is instead of jumping.
 */
class SLATERUI_API FSlaterAnimator
{
public:
	/** Jumps to Value and stops. */
	void Snap(float Value);

	/** Moves from the current value to Target in Duration seconds. A duration of 0 or less snaps. */
	void Start(float Target, float Duration, ECurveEaseFunction Ease);

	/** Adds DeltaTime. True while it is still moving afterwards. */
	bool Advance(float DeltaTime);

	float GetValue() const;
	float GetTarget() const { return To; }
	bool IsActive() const { return bActive; }
	/** False until the first `Snap` or `Start`: there is no value to move from. */
	bool HasValue() const { return bHasValue; }
	/** Forgets the value, so the next `Start` is a snap. */
	void Reset();

private:
	float From = 0.f;
	float To = 0.f;
	float Elapsed = 0.f;
	float Length = 0.f;
	ECurveEaseFunction Ease = ECurveEaseFunction::Linear;
	bool bActive = false;
	bool bHasValue = false;
};
