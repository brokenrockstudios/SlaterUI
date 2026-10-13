// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "SlaterList.h"
#include "SlaterView.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"

/** Distinct transient textures named icon0..iconN-1 plus `lock`, each its own UObject and render resource, so every icon breaks a draw batch as it does in the real grid. */
class FSlaterTestImages : public ISlaterImageSource
{
public:
	explicit FSlaterTestImages(int32 NumIcons)
	{
		for (int32 I = 0; I <= NumIcons; ++I)
		{
			// A solid color per icon (hue by index; the lock is dark), so a rendered grid is readable.
			const FColor Color = I == NumIcons ? FColor(30, 30, 30, 255) : FLinearColor::MakeFromHSV8(uint8(I * 255 / NumIcons), 200, 230).ToFColor(true);
			TArray<uint8> Pixels;
			for (int32 P = 0; P < 64; ++P)
			{
				Pixels.Add(Color.B);
				Pixels.Add(Color.G);
				Pixels.Add(Color.R);
				Pixels.Add(255);
			}
			UTexture2D* Texture = UTexture2D::CreateTransient(8, 8, PF_B8G8R8A8, NAME_None, Pixels);
			Texture->UpdateResource();
			Keep.Emplace(Texture);
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(8.0, 8.0);
			Brushes.Add(I == NumIcons ? FName(TEXT("lock")) : FName(*FString::Printf(TEXT("icon%d"), I)), Brush);
		}
	}

	virtual const FSlateBrush* FindImage(FName Name) const override { return Brushes.Find(Name); }

	TMap<FName, FSlateBrush> Brushes;
	TArray<TStrongObjectPtr<UTexture2D>> Keep;
};

namespace SlaterListKit
{
	/** Mock data: cells 0..NumCells-1 in state `normal`, NumItems items spread over the cells (every fifth locked), icons cycling over NumIcons. */
	inline FString MakeMockJson(int32 NumCells, int32 NumItems, int32 NumIcons = 16)
	{
		FString Json = TEXT("{\"Title\":\"Backpack\",\"Cells\":[");
		for (int32 I = 0; I < NumCells; ++I)
		{
			Json += FString::Printf(TEXT("%s{\"Index\":%d,\"State\":\"normal\"}"), I ? TEXT(",") : TEXT(""), I);
		}
		Json += TEXT("],\"Items\":[");
		const int32 Stride = FMath::Max(1, NumCells / FMath::Max(1, NumItems));
		for (int32 N = 0; N < NumItems; ++N)
		{
			Json += FString::Printf(TEXT("%s{\"Id\":\"i%d\",\"Slot\":%d,\"Icon\":\"icon%d\",\"Count\":%d,\"Locked\":%s}"), N ? TEXT(",") : TEXT(""), N, N * Stride, N % NumIcons, N + 1, (N % 5 == 0) ? TEXT("true") : TEXT("false"));
		}
		Json += TEXT("]}");
		return Json;
	}

	/** The shipped ListProbe layout with another column count. */
	inline FString LayoutText(int32 Columns)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Slater::FindLayoutFile(TEXT("ListProbe")));
		return Text.Replace(TEXT("columns=\"10\""), *FString::Printf(TEXT("columns=\"%d\""), Columns));
	}

	/** First widget in the tree (Root included) of this Slate type, or null. */
	inline TSharedPtr<SWidget> Find(const TSharedRef<SWidget>& Root, const TCHAR* Type)
	{
		if (Root->GetType() == FName(Type))
		{
			return Root;
		}
		TSharedPtr<SWidget> Found;
		Root->GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			if (!Found.IsValid())
			{
				Found = Find(Child.AsShared(), Type);
			}
		});
		return Found;
	}

	/** A layout built against Base, bound to a model in a mode. */
	struct FRig
	{
		FSlaterStyleStack Stack;
		TSharedPtr<FSlaterTestImages> Images;
		TSharedPtr<FSlaterModel> Model;
		FSlaterBuildResult Result;
		TSharedPtr<FSlaterBinder> Binder;
		FString Errors;

		bool Init(const FString& Layout, const FString& Json, ESlaterBindMode Mode)
		{
			Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
			if (!Images.IsValid())
			{
				Images = MakeShared<FSlaterTestImages>(16);
			}
			Slater::SetImageSource(Images);
			Model = MakeShared<FSlaterModel>();
			FSlaterError Error;
			if (!Model->LoadMock(Json, TEXT("mock"), Error))
			{
				Errors = Error.ToString();
				return false;
			}
			if (!Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result))
			{
				for (const FSlaterError& E : Result.Errors)
				{
					Errors += E.ToString() + TEXT("\n");
				}
				return false;
			}
			Binder = Slater::Bind(Result, Model.ToSharedRef(), Mode, Stack);
			return true;
		}

		void Shutdown()
		{
			Binder.Reset();
			Result = FSlaterBuildResult();
			Model.Reset();
			Slater::SetImageSource(nullptr);
			Images.Reset();
		}

		TSharedPtr<SSlaterRepeat> Repeat(int32 Index) const
		{
			if (!Result.Repeats.IsValidIndex(Index))
			{
				return nullptr;
			}
			return StaticCastSharedPtr<SSlaterRepeat>(Result.Repeats[Index].Pin());
		}
	};
}

#endif
