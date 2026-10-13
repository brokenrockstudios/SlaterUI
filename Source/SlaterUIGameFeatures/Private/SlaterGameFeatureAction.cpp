// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterGameFeatureAction.h"

#include "SlaterSlotHost.h"
#include "SlaterView.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFeatureData.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlaterGameFeatureAction)

#define LOCTEXT_NAMESPACE "SlaterUIGameFeatures"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterGameFeatures, Log, All);

// USlaterGameFeatureAction_AddLayout

void USlaterGameFeatureAction_AddLayout::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	Super::OnGameFeatureActivating(Context);
	Activate();
}

void USlaterGameFeatureAction_AddLayout::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);
	Deactivate();
}

void USlaterGameFeatureAction_AddLayout::Activate()
{
	if (bActive)
	{
		return;
	}
	bActive = true;
	AddPluginRoots();

	WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddWeakLambda(this, [this](UWorld* World, const UWorld::InitializationValues)
	{
		if (World && World->IsGameWorld())
		{
			AddToWorld(*World);
		}
	});
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddWeakLambda(this, [this](UWorld* World, bool, bool)
	{
		for (int32 I = Worlds.Num() - 1; I >= 0; --I)
		{
			if (Worlds[I].World.Get() == World || !Worlds[I].World.IsValid())
			{
				RemoveWorld(I);
			}
		}
	});
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World(); World && World->IsGameWorld())
			{
				AddToWorld(*World);
			}
		}
	}
}

void USlaterGameFeatureAction_AddLayout::Deactivate()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldInitHandle.Reset();
	WorldCleanupHandle.Reset();

	for (int32 I = Worlds.Num() - 1; I >= 0; --I)
	{
		RemoveWorld(I);
	}	RemovePluginRoots();
}

void USlaterGameFeatureAction_AddLayout::AddToWorld(UWorld& World)
{
	if (!bActive || World.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (const FWorldData& Data : Worlds)
	{
		if (Data.World.Get() == &World)
		{
			return;
		}
	}
	const TSharedPtr<ISlaterSlotHost> Host = Slater::GetSlotHost();
	if (!Host)
	{
		UE_LOG(LogSlaterGameFeatures, Warning, TEXT("Add Slater Layout: no slot host is set (Slater::SetSlotHost), so no layout is shown. The SlaterUIExtension plugin provides one."));
		return;
	}

	FWorldData& Data = Worlds.AddDefaulted_GetRef();
	Data.World = &World;
	for (const FSlaterLayoutSlotEntry& Entry : Layouts)
	{
		if (!Entry.SlotID.IsValid() || !Slater::IsValidLayoutId(Entry.LayoutId))
		{
			UE_LOG(LogSlaterGameFeatures, Warning, TEXT("Add Slater Layout: skipped '%s' in slot '%s' (needs a slot tag and a valid layout ID)"), *Entry.LayoutId, *Entry.SlotID.ToString());
			continue;
		}
		if (const uint32 Token = Host->AddLayout(World, Entry.SlotID, Entry.LayoutId, Entry.StyleName))
		{
			Data.Tokens.Add(Token);
		}
	}
}

void USlaterGameFeatureAction_AddLayout::RemoveWorld(int32 Index)
{
	if (UWorld* World = Worlds[Index].World.Get())
	{
		if (const TSharedPtr<ISlaterSlotHost> Host = Slater::GetSlotHost())
		{
			for (const uint32 Token : Worlds[Index].Tokens)
			{
				Host->RemoveLayout(*World, Token);
			}
		}
	}
	Worlds.RemoveAtSwap(Index);
}
int32 USlaterGameFeatureAction_AddLayout::GetRegisteredCount() const
{
	int32 Count = 0;
	for (const FWorldData& Data : Worlds)
	{
		Count += Data.Tokens.Num();
	}
	return Count;
}

void USlaterGameFeatureAction_AddLayout::AddPluginRoots()
{
	const UGameFeatureData* Data = GetGameFeatureData();
	if (!Data)
	{
		return;
	}
	const FName Mount = FPackageName::GetPackageMountPoint(Data->GetOutermost()->GetName());
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Mount.ToString());
	if (!Plugin)
	{
		return;
	}

	const FString LayoutsDir = FPaths::Combine(Plugin->GetContentDir(), TEXT("UI/Layouts"));
	const FString Styles = FPaths::Combine(Plugin->GetContentDir(), TEXT("UI/Styles"));
	if (IFileManager::Get().DirectoryExists(*LayoutsDir))
	{
		Slater::AddLayoutRoot(LayoutsDir);
		AddedLayoutRoots.Add(LayoutsDir);
	}
	if (IFileManager::Get().DirectoryExists(*Styles))
	{
		Slater::AddStyleRoot(Styles);
		AddedStyleRoots.Add(Styles);
		TArray<FSlaterError> Errors;
		Slater::ReloadStyles(Errors);
	}
}

void USlaterGameFeatureAction_AddLayout::RemovePluginRoots()
{
	for (const FString& Root : AddedLayoutRoots)
	{
		Slater::RemoveLayoutRoot(Root);
	}
	const bool bStylesChanged = AddedStyleRoots.Num() > 0;
	for (const FString& Root : AddedStyleRoots)
	{
		Slater::RemoveStyleRoot(Root);
	}
	AddedLayoutRoots.Reset();
	AddedStyleRoots.Reset();
	if (bStylesChanged)
	{
		TArray<FSlaterError> Errors;
		Slater::ReloadStyles(Errors);
	}
}

#if WITH_EDITOR
EDataValidationResult USlaterGameFeatureAction_AddLayout::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	int32 Index = 0;
	for (const FSlaterLayoutSlotEntry& Entry : Layouts)
	{
		if (!Slater::IsValidLayoutId(Entry.LayoutId))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("BadLayoutId", "LayoutId '{1}' at index {0} is not a layout ID (letters, digits, _ and - in segments joined by /)"), FText::AsNumber(Index), FText::FromString(Entry.LayoutId)));
		}
		if (!Entry.SlotID.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NoSlot", "SlotID is not set at index {0}"), FText::AsNumber(Index)));
		}
		++Index;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
