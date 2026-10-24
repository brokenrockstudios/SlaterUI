// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterImages.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Brushes/SlateImageBrush.h"

namespace SlaterImagesPrivate
{
	class FSlaterFileImages : public ISlaterImageSource
	{
	public:
		FSlaterFileImages(const FString& Directory, float ImageSize)
		{
			TArray<FString> Files;
			IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*.svg")), true, false);
			for (const FString& File : Files)
			{
				Brushes.Add(FName(*FPaths::GetBaseFilename(File)), FSlateVectorImageBrush(FPaths::ConvertRelativePathToFull(Directory / File), FVector2f(ImageSize, ImageSize)));
			}
		}

		virtual const FSlateBrush* FindImage(FName Name) const override { return Brushes.Find(Name); }

	private:
		TMap<FName, FSlateVectorImageBrush> Brushes;
	};
}
using namespace SlaterImagesPrivate;

TSharedRef<const ISlaterImageSource> Slater::MakeFileImageSource(const FString& Directory, float ImageSize)
{
	return MakeShared<FSlaterFileImages>(Directory, ImageSize);
}

TSharedRef<const ISlaterImageSource> Slater::MakePluginIconSource()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SlaterUI"));
	return MakeFileImageSource(Plugin.IsValid() ? Plugin->GetContentDir() / TEXT("UI/Icons") : FString());
}
