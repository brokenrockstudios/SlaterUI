// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWidget.h"

/**
 * Plain-Slate side of the agent fluency probe (T-355, Docs/SlaterUI/AgentProbe). Each CppTaskN.cpp implements one
 * function; the stubs return SNullWidget until an agent writes the task. `BRS.SlaterUI.AgentProbe.Cpp` renders them.
 */
namespace SlaterProbeCpp
{
	/** The Base style's values, for the theme task (Task5) and as a reference when reading the others. */
	struct FProbeTheme
	{
		FLinearColor Panel = FLinearColor::White;
		FLinearColor Border = FLinearColor::White;
		FLinearColor Text = FLinearColor::White;
		FLinearColor TextMuted = FLinearColor::White;
		FLinearColor Accent = FLinearColor::White;
		FLinearColor Danger = FLinearColor::White;
		float SpaceS = 4.f;
		float SpaceM = 8.f;
		float SpaceL = 16.f;
		float Radius = 4.f;
		float BorderThin = 1.f;
		FSlateFontInfo Body;
		FSlateFontInfo Heading;
		FSlateFontInfo Caption;

		static FProbeTheme MakeBase()
		{
			FProbeTheme Theme;
			Theme.Panel = FLinearColor::FromSRGBColor(FColor(0xFF, 0xFF, 0xFF));
			Theme.Border = FLinearColor::FromSRGBColor(FColor(0xC9, 0xCD, 0xD4));
			Theme.Text = FLinearColor::FromSRGBColor(FColor(0x1D, 0x21, 0x29));
			Theme.TextMuted = FLinearColor::FromSRGBColor(FColor(0x6B, 0x72, 0x80));
			Theme.Accent = FLinearColor::FromSRGBColor(FColor(0x2F, 0x6F, 0xEB));
			Theme.Danger = FLinearColor::FromSRGBColor(FColor(0xD6, 0x45, 0x45));
			Theme.Body = FCoreStyle::GetDefaultFontStyle("Regular", 14);
			Theme.Heading = FCoreStyle::GetDefaultFontStyle("Bold", 20);
			Theme.Caption = FCoreStyle::GetDefaultFontStyle("Italic", 11);
			return Theme;
		}
	};

	/** Task 1: a panel matching a reference PNG. */
	TSharedRef<SWidget> Task1();
	/** Task 2: a 4x4 grid of counts; OutSetCount(Index, Count) changes one cell's count later. */
	TSharedRef<SWidget> Task2(TFunction<void(int32, int32)>& OutSetCount);
	/** Task 3: the given panel restyled. */
	TSharedRef<SWidget> Task3();
	/** Task 4: a file that does not compile, repaired. */
	TSharedRef<SWidget> Task4();
	/** Task 5: a snippet ported to a theme (or translated). */
	TSharedRef<SWidget> Task5(const FProbeTheme& Theme);
}

#endif
