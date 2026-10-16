// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class SSlaterView;
class UWorld;

/**
 * One worked example: a layout under `Content/UI/Layouts/Examples/` and the C++ that binds to it. Create builds a view in
 * the given style with its data, commands and outlets already set, and the view keeps all of it alive.
 */
struct SLATERUIEXAMPLES_API FSlaterExample
{
	FName Name;
	/** The layout ID the example shows (`Examples/Hello`). */
	FString LayoutId;
	/** One line for `Slater.Example`'s list: what the example teaches. */
	FString Summary;
	/** Builds the view for a window. Null for an example that is a widget (see Show). */
	TFunction<TSharedRef<SSlaterView>(FName StyleName)> Create;
	/** For a widget example: makes the widget in the world and adds it to the viewport. Null for a plain view (see Create). */
	TFunction<void(UWorld& World)> Show;
};

/** The examples by name. The module registers its own at startup; a test or a game can add more. Game thread only. */
class SLATERUIEXAMPLES_API FSlaterExampleRegistry
{
public:
	static FSlaterExampleRegistry& Get();

	/** False for an empty name or a name that is already registered. */
	bool Register(FSlaterExample Example);
	bool Unregister(FName Name);
	/** The example by name (case-insensitive, as `FName` is), or null. */
	const FSlaterExample* Find(FName Name) const;
	/** Registered names in the order they were added. */
	void GetNames(TArray<FName>& OutNames) const;

private:
	TArray<FSlaterExample> Examples;
};

namespace SlaterExamples
{
	/**
	 * Hello: a counter. No view-model class at all: `FSlaterGetterScope` holds getters by path, the command bumps a number
	 * and tells the scope which paths changed.
	 */
	SLATERUIEXAMPLES_API TSharedRef<SSlaterView> MakeHello(FName StyleName);
}
