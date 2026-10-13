// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SlaterList.h"
#include "SlaterTestViewModels.h"
#include "SlaterView.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Text/STextBlock.h"

namespace SlaterBindKit
{
	inline void CollectTexts(SWidget& Widget, TArray<FString>& Out)
	{
		if (Widget.GetType() == FName(TEXT("STextBlock")))
		{
			Out.Add(static_cast<STextBlock&>(Widget).GetText().ToString());
		}
		Widget.GetChildren()->ForEachWidget([&](SWidget& Child) { CollectTexts(Child, Out); });
	}

	/** The text of every text block under the view, in tree order. */
	inline TArray<FString> Texts(const TSharedRef<SWidget>& Root)
	{
		TArray<FString> Texts;
		CollectTexts(*Root, Texts);
		return Texts;
	}

	inline FString TextOf(const TSharedPtr<SWidget>& Widget)
	{
		return (Widget.IsValid() && Widget->GetType() == FName(TEXT("STextBlock"))) ? StaticCastSharedPtr<STextBlock>(Widget)->GetText().ToString() : FString();
	}

	inline TSharedPtr<SSlaterRepeat> Repeat(const TSharedRef<SSlaterView>& View, int32 Index = 0)
	{
		const FSlaterBuildResult& Result = View->GetResult();
		return Result.Repeats.IsValidIndex(Index) ? StaticCastSharedPtr<SSlaterRepeat>(Result.Repeats[Index].Pin()) : nullptr;
	}

	inline const TCHAR* const ItemsLayout = TEXT("<vbox><repeat source=\"{Items}\" key=\"Id\" columns=\"1\"><text>{Count}</text></repeat></vbox>");
}

/** A temp layout folder and view models kept alive for one test. */
struct FSlaterBindFixture
{
	FString Dir;
	TArray<TStrongObjectPtr<UObject>> Keep;

	void Init()
	{
		Dir = FPaths::ProjectSavedDir() / TEXT("SlaterUI/BindTests");
		IFileManager::Get().MakeDirectory(*(Dir / TEXT("Layouts")), true);
		Slater::AddLayoutRoot(Dir / TEXT("Layouts"));
	}

	void Shutdown()
	{
		Slater::RemoveLayoutRoot(Dir / TEXT("Layouts"));
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		Keep.Reset();
	}

	void Write(const TCHAR* Name, const FString& Text) const
	{
		FFileHelper::SaveStringToFile(Text, *(Dir / TEXT("Layouts") / Name));
	}

	template <typename T>
	T* Make()
	{
		T* Object = NewObject<T>(GetTransientPackage());
		Keep.Emplace(Object);
		return Object;
	}

	USlaterTestItemVM* MakeItem(const TCHAR* Id, int32 Count)
	{
		USlaterTestItemVM* Item = Make<USlaterTestItemVM>();
		Item->Id = Id;
		Item->Count = Count;
		return Item;
	}

	USlaterTestPlayerVM* MakePlayer(const TCHAR* Name)
	{
		USlaterTestPlayerVM* Player = Make<USlaterTestPlayerVM>();
		Player->Name = Name;
		return Player;
	}
};


#endif
