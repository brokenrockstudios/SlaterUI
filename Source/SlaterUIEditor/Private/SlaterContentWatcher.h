// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "IDirectoryWatcher.h"

/**
 * Watches the folders layouts and styles are read from and tells SlaterUI when `.sltr` or `.sltrstyle` files change
 * (`Slater::NotifyFilesChanged`). A save usually reports several events, so changes are collected and passed on once
 * after a short quiet time.
 */
class FSlaterContentWatcher
{
public:
	~FSlaterContentWatcher() { Stop(); }

	/** Watches the folders that exist now. Call `Rescan` after a folder is created. */
	void Start();
	void Stop();
	/** Drops the watches and registers the folders that exist now. */
	void Rescan();

	int32 NumWatchedFolders() const { return Watches.Num(); }

private:
	struct FWatch
	{
		FString Directory;
		FDelegateHandle Handle;
	};

	TArray<FWatch> Watches;
	TSet<FString> Pending;
	FTSTicker::FDelegateHandle FlushHandle;

	void OnDirectoryChanged(const TArray<FFileChangeData>& Changes);
	bool Flush(float DeltaTime);
};
