# Achievement System for Unreal Engine

Data-driven achievements for **Unreal Engine 5.8** with **host-authoritative rewards**, **Steam sync** and **split-screen** support. It is built for P2P / listen-server multiplayer games.

Designers create achievements as Data Assets. Gameplay code reports what happened with one call. The plugin handles progress, unlocks, rewards, replication, saving and Steam.

> **Status: 0.2.0 (pre-release).** Everything is implemented and was exercised in PIE. It has not yet been validated in a packaged Shipping build, against a live Steam AppID or across two machines. Test it in your project before relying on it in production.

Full documentation (Portuguese) ships with the plugin in [`Docs/index.html`](Docs/index.html). In the editor, open it from **Tools → Achievements → Documentation**.

---

## Features

- **Data-driven definitions.** Achievements are `AchievementDefinition` Data Assets: gameplay-tag Id, Steam API name, one-shot or incremental targets, prerequisites, conditions and rewards. They are validated on save.
- **Event-based gameplay API.** Gameplay says "this happened" (`Event.Enemy.Killed`), and every achievement listening to that event advances.
- **Host-authoritative multiplayer.**
  - The host evaluates progress and grants rewards.
  - Progress replicates to the owning client only (FastArray deltas).
  - Each player syncs to their own Steam account.
- **Rewards that are never granted twice.**
  - Rewards are tracked per index, so partial grants are safe to retry.
  - A grant can return `Granted`, `Deferred` (retried automatically by `RetryPendingRewards`) or `Failed` (manual retry only).
  - Rewards are claimed either automatically on unlock or manually by the player.
- **Extensible from C++ or Blueprint.** You can write your own conditions, rewards and reward granters. The core has no dependency on your inventory.
- **Steam integration.**
  - Unlocks and stats go through a persisted queue with retry.
  - On startup the plugin reconciles with Steam, which restores achievements after a reinstall.
  - Steam's native progress popup is supported.
  - Other platforms plug in through `IAchievementPlatform`.
- **Split-screen.** Each local player gets their own save. Only the primary player syncs with Steam.
- **Save safety.**
  - Progress never regresses: both the save and the host merge with max.
  - A backup slot is restored automatically when the main save is missing or corrupt.
- **UI-agnostic API.** `UAchievementLocalPlayerSubsystem` gives any UI the events, ready-made view data, filters and claims, in matches and in menus. See [Building your own UI](#building-your-own-ui).
- **Optional UI base classes.** A queued toast, an achievement list and an entry widget with async icons, reward icons, masking for hidden achievements and a claim button. You build the visuals in UMG.
- **Tooling.**
  - Validation and a Steamworks export sheet under **Tools → Achievements**.
  - Debug console commands.
  - Automation tests.

## Requirements

| | |
|---|---|
| Engine | Unreal Engine 5.8 |
| Platforms | Any platform for the core. The Steam module builds on Win64, Linux and Mac. |
| Plugins | `OnlineSubsystemSteam` (enabled automatically) |
| Steam | A Steamworks AppID with achievements configured, for live sync. Without Steam the plugin falls back to a local *Null* platform. |

## Installation

1. Clone or download this repository into your project's `Plugins` folder:
   ```bash
   git clone https://github.com/ricardofuly/AchievementSystem.git Plugins/AchievementSystem
   ```
2. Regenerate project files, build from your IDE and enable the plugin.
3. In **Project Settings → Asset Manager → Primary Asset Types to Scan**, add an entry:
   - **Primary Asset Type:** `AchievementDefinition` (exactly this name);
   - **Asset Base Class:** `AchievementDefinition`;
   - **Directories:** the folder that holds your definitions, e.g. `/Game/Achievements`.

   Restart the editor after changing it.
4. Add `AchievementComponent` to your **PlayerState**.

## Quick start

1. Create the gameplay tags `Achievement.Tutorial.Done` and `Event.Tutorial.Completed`.
2. Create an `AchievementDefinition` Data Asset:
   - `Id` = `Achievement.Tutorial.Done`;
   - `ListenedEvents` = `Event.Tutorial.Completed`;
   - `TargetValue` = `1`;
   - `PlatformApiName` = your Steamworks API name.
3. Report the event **on the host**:

   ```cpp
   #include "AchievementStatics.h"

   UAchievementStatics::ReportAchievementEvent(PlayerState, TAG_Event_Tutorial_Completed, 1, FGameplayTagContainer());
   ```

   In Blueprint, use **Report Achievement Event** (or **Report Achievement Event For All**).
4. Run PIE with 2 players as *Listen Server* and type `Achievements.SelfTest` in the host console.

## Rewards

A reward has two parts:

- a **reward** (`UAchievementReward`), which is data only and describes what to give;
- a **granter** (`IAchievementRewardGranter`), which is your code and actually delivers it.

Map each reward class to a granter in **Project Settings → Plugins → Achievements → Default Granters**, or call `RegisterRewardGranter` from code. Granters run on the host only.

| Granter result | Meaning | What happens |
|---|---|---|
| `Granted` | Delivered | Marked as granted, never repeated |
| `Deferred` | Temporary blocker (e.g. full inventory) | Stays pending. Delivered by `RetryPendingRewards()`, a manual claim or the next session |
| `Failed` | Error (e.g. missing component) | Stays pending. Skipped by `RetryPendingRewards()`; only a manual claim or the next session retries it |

A working reference is in [`Examples/AchievementExampleWallet.h`](Source/AchievementsCore/Public/Examples/AchievementExampleWallet.h): a replicated wallet component plus the currency granter that feeds it.

> The plugin persists *that a reward was granted*, not the item itself. Your inventory or wallet must save its own contents.

## Building your own UI

The widgets in `AchievementsUI` are an **optional** reference implementation. Any UI (plain UMG, CommonUI, MVVM, Slate or a HUD) can use the same public API: `UAchievementLocalPlayerSubsystem`, one per local player.

- **Events are always bindable**, even before the PlayerState replicates: `OnAchievementsReady`, `OnProgressChanged`, `OnAchievementUnlocked`, `OnRewardsClaimable`, `OnRewardsClaimed`.
- **Queries work in menus too.** In a match they read live progress; elsewhere they read the local save.
  - `GetAchievements(Filter, Sort)`
  - `GetAchievement(Id)`
  - `GetUnlockedCount()` / `GetTotalCount()`
- **Commands:** `ClaimRewards(Id)` and `ClaimAllRewards()`.
- **Display-ready data:** `FAchievementViewData` comes with progress from 0 to 1, rewards, `bClaimable` and hidden achievements masked.

```cpp
#include "AchievementLocalPlayerSubsystem.h"

UAchievementLocalPlayerSubsystem* Achievements = LocalPlayer->GetSubsystem<UAchievementLocalPlayerSubsystem>();
Achievements->OnAchievementUnlocked.AddDynamic(this, &UMyHud::HandleUnlocked);

for (const FAchievementViewData& View : Achievements->GetAchievements(EAchievementListFilter::Claimable))
{
	// Build your rows.
}
```

In Blueprint: **Get Owning Local Player → Get Subsystem (AchievementLocalPlayerSubsystem)**, then bind the events and call **Get Achievements**.

## Architecture

| Module | Type | Responsibility |
|---|---|---|
| `AchievementsCore` | Runtime | Definitions, component, subsystems, rewards, save and platform interfaces. No Steam dependency. |
| `AchievementsSteam` | Runtime | Steamworks implementation of `IAchievementPlatform` |
| `AchievementsUI` | Runtime | Optional toast, list and entry widget base classes |
| `AchievementsEditor` | Editor | Validation, Steamworks export and the documentation tab |
| `AchievementsTests` | Editor | Automation Specs |

How a session works:

1. Each player loads their local save and sends a snapshot to the host. The host merges it and never lowers progress.
2. Gameplay reports events on the host. Achievements advance, unlock and grant rewards.
3. Progress replicates to the owner only. The owner saves it locally and queues it for Steam.

Main classes:

| Class | Role |
|---|---|
| `UAchievementSubsystem` (Game Instance) | Definition registry, reward granters, platform |
| `UAchievementLocalPlayerSubsystem` (Local Player) | UI API (events, queries, claims), per-player save and platform sync |
| `UAchievementComponent` (PlayerState) | Host-side progress, replication, claims |
| `UAchievementStatics` | One-line Blueprint/C++ gameplay API |

## Tools

**Tools → Achievements** in the editor:

- **Validate Achievements**: checks every definition for duplicate Ids or API names, missing events, empty rewards and self prerequisites.
- **Export Steamworks Sheet**: writes `Saved/Achievements/SteamworksAchievements.csv` to fill the Steamworks partner site.
- **Documentation**: opens the bundled documentation in an editor tab.

Console commands (not available in Shipping builds):

| Command | Where | What |
|---|---|---|
| `Achievements.SelfTest [Id]` | host | Unlocks one achievement for everyone and checks host state, rewards, replication and client saves |
| `Achievements.Report <EventTag> [Amount]` | host | Reports an event for every player |
| `Achievements.Claim [Id]` | any | Claims that window's pending rewards |
| `Achievements.Dump` | any | Lists definitions, the active platform and visible progress |
| `Achievements.Reset` | any | Wipes local saves (and the primary player's Steam achievements). Restart PIE afterwards |

## Tests

The `AchievementsTests` module contains Automation Specs grouped under `AchievementSystem.*`:

- **Merge**: progress never regresses.
- **Storage**: save, backup, corrupt-save recovery, Null platform.
- **Progress**: events, targets, prerequisites, conditions.
- **Rewards**: auto and manual claim, `Deferred`/`Failed`, no double grant.

Run them from **Tools → Test Automation** (filter `AchievementSystem`), or headless:

```bash
UnrealEditor-Cmd.exe YourProject.uproject -ExecCmds="Automation RunTests AchievementSystem;Quit" -unattended -nullrhi -nosplash -log
```

The tests cover host-side logic and persistence. Replication, split-screen and Steam still need the in-game `Achievements.SelfTest`.

## Known limitations

- **P2P trust.** Each player owns their save; the host validates bounds but cannot prevent save editing. Rewards with real-world value need your own backend.
- **Split-screen guests** are saved by local player position (`_P1`, `_P2`...), not by account, and do not sync to Steam.
- **Definitions load synchronously** at startup. This is fine for typical counts but not yet asynchronous.
- **Steamworks SDK 1.61+** is assumed: stats are fetched automatically at `SteamAPI_Init`.

## Contributing

Issues and pull requests are welcome at [github.com/ricardofuly/AchievementSystem](https://github.com/ricardofuly/AchievementSystem/issues). Please run the `AchievementSystem` automation tests before submitting.

## License

Copyright 2026 Ricardo Fuly.

Licensed under the [Apache License, Version 2.0](LICENSE). See [NOTICE](NOTICE) for attribution.

Steamworks is a trademark of Valve Corporation and Unreal Engine is a trademark of Epic Games, Inc. This project is not affiliated with or endorsed by either company.
