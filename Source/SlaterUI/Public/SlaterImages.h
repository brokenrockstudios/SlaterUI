// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterList.h"

/**
 * An `ISlaterImageSource` that reads icons from files, so a sample layout can show pictures without a `.uasset` (T-403).
 * Every `*.svg` in the folder is an image named after the file without its extension (`Home.svg` is `<image src="Home"/>`),
 * drawn through the engine's file-based vector brush (`FSlateVectorImageBrush`, rasterized by Slate at the size it is drawn,
 * tinted by the brush color; the stock icons are white strokes for a colored button). PNG files are not read.
 */
namespace Slater
{
	/** Reads every `*.svg` directly in Directory, once, at the size ImageSize. A file that cannot be listed is skipped. */
	SLATERUI_API TSharedRef<const ISlaterImageSource> MakeFileImageSource(const FString& Directory, float ImageSize = 24.f);

	/** The plugin's own icons, `Content/UI/Icons`. */
	SLATERUI_API TSharedRef<const ISlaterImageSource> MakePluginIconSource();
}
