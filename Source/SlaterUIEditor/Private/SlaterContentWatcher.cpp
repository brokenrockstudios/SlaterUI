// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterContentWatcher.h"

#include "DirectoryWatcherModule.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SlaterView.h"

namespace
{
	/** Quiet time after the last event before the change is passed on. */
	constexpr float FlushDelaySeconds = 0.25f;

	void GetWatchedDirectories(TArray<FString>& OutDirectories)
	{
		Slater::GetLayoutRoots(OutDirectories);
		OutDirectories.Add(FPaths::ProjectContentDir() / TEXT("UI/Styles"));
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SlaterUI")))
		{
			OutDirectories.Add(Plugin->GetContentDir() / TEXT("UI/Styles"));
		}
	}
}

void FSlaterContentWatcher::Start()
{
	Rescan();
}

void FSlaterContentWatcher::Stop()
{
	if (FDirectoryWatcherModule* Module = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
	{
		if (IDirectoryWatcher* Watcher = Module->Get())
		{
			for (const FWatch& Watch : Watches)
			{
				Watcher->UnregisterDirectoryChangedCallback_Handle(Watch.Directory, Watch.Handle);
			}
		}
	}
	Watches.Reset();
	Pending.Reset();
	if (FlushHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FlushHandle);
		FlushHandle.Reset();
	}
}

void FSlaterContentWatcher::Rescan()
{
	Stop();
	FDirectoryWatcherModule& Module = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* Watcher = Module.Get();
	if (!Watcher)
	{
		return;
	}
	TArray<FString> Directories;
	GetWatchedDirectories(Directories);
	for (const FString& Directory : Directories)
	{
		if (!IFileManager::Get().DirectoryExists(*Directory))
		{
			continue;
		}
		FWatch Watch;
		Watch.Directory = Directory;
		if (Watcher->RegisterDirectoryChangedCallback_Handle(Directory, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FSlaterContentWatcher::OnDirectoryChanged), Watch.Handle))
		{
			Watches.Add(MoveTemp(Watch));
		}
	}
}

void FSlaterContentWatcher::OnDirectoryChanged(const TArray<FFileChangeData>& Changes)
{
	for (const FFileChangeData& Change : Changes)
	{
		Pending.Add(Change.Filename);
	}
	if (Pending.Num() == 0)
	{
		return;
	}
	// Restart the quiet time on every event.
	if (FlushHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FlushHandle);
	}
	FlushHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FSlaterContentWatcher::Flush), FlushDelaySeconds);
}

bool FSlaterContentWatcher::Flush(float DeltaTime)
{
	FlushHandle.Reset();
	const TArray<FString> Files = Pending.Array();
	Pending.Reset();
	Slater::NotifyFilesChanged(Files);
	return false;
}
