# Target Architecture

Clean Architecture (Robert Martin): game rules live in plain C++ modules, use cases coordinate those rules through narrow ports, and Godot classes stay as humble adapters that translate engine events into use-case calls and use-case output into nodes, visuals, audio, and scene navigation.

The goal is the important behavior to be easy to test without Godot while keeping the Godot-facing code straightforward.

## Clean Architecture Dependency Rule

Source dependencies point inward. Inner code must not know about outer code.

```mermaid
flowchart TB
    subgraph Drivers[Frameworks and Drivers]
        Godot[Godot scenes, nodes, signals, timers]
        Assets[Assets, audio buses, resources]
        Files[FileAccess and user save path]
    end

    subgraph Adapters[Interface Adapters]
        GodotAdapters[Godot view and node adapters]
        JsonAdapters[JSON repositories and parsers]
        Presenters[Presenters and view-model builders]
        Facades[Godot singletons and script-facing facades]
    end

    subgraph UseCases[Application Use Cases]
        MatchFlow[Match flow]
        CombatFlow[Combat flow]
        CampaignFlow[Campaign and rewards]
        MenuFlow[Menu, settings, navigation decisions]
    end

    subgraph Domain[Entities and Domain Rules]
        MatchRules[Match state and scoring]
        CombatRules[Combat rules]
        ProgressionRules[Progression rules]
        ContentModels[Unit, level, upgrade, menu models]
    end

    Drivers --> Adapters
    Adapters --> UseCases
    UseCases --> Domain

    Adapters -. implement .-> Ports[Use-case ports]
    UseCases --> Ports
```

Dependency rules:

- Domain code contains deterministic rules and data models. It must not derive from `Object`, `Node`, `Node2D`, `CanvasLayer`, or any other Godot class.
- Use cases coordinate domain objects and depend on ports, not concrete repositories, Godot singletons, scene trees, timers, or nodes.
- Interface adapters translate between Godot/JSON concepts and use-case/domain concepts.
- Framework code owns Godot lifecycle, file access, resources, node construction, and engine callbacks.
- Cross-boundary calls pass value objects, IDs, snapshots, commands, and intents rather than `Node *` whenever possible.
- Godot value types such as `String`, `Vector2`, and `Color` should not be named from within inner circles. The model should prefer engine-neutral value types for code that must be native-testable without Godot startup.

## Humble Object Policy

A humble object is allowed to know the framework but should contain as little decision-making as possible. In this project, all Godot nodes should trend humble.

Humble Godot objects should:

- create and own nodes;
- connect and emit signals;
- read input and lifecycle callbacks;
- collect snapshots from the scene;
- call use cases;
- apply returned intents to movement, animation, health, VFX, audio, UI, or navigation.

Humble Godot objects should not:

- decide campaign unlocks, scoring, upgrade eligibility, target priority, combat timing, deployment affordability, or wave completion;
- parse business data directly into rule objects without a parser/repository boundary;
- own random selection logic that cannot be seeded in tests;
- require native tests to instantiate Godot nodes just to test rules.

## Target Source Layout

The important part is ownership and dependency direction.

```mermaid
flowchart TB
    Src[defn/src]

    Src --> Domain[domain]
    Domain --> DomainMatch[match]
    Domain --> DomainCombat[combat]
    Domain --> DomainProgression[progression]
    Domain --> DomainContent[content]

    Src --> App[application]
    App --> AppPorts[ports]
    App --> AppUseCases[use_cases]

    Src --> Adapters[adapters]
    Adapters --> GodotAdapter[godot]
    Adapters --> JsonAdapter[json]
    Adapters --> PresenterAdapter[presenters]

    Src --> Framework[framework]
    Framework --> GodotNodes[godot_nodes]
    Framework --> Registration[register_types]

    Src --> TestsSupport[test_support]
```

Recommended directory intent:

- `domain/`: pure rules and data structures.
- `application/`: use-case classes, input/output DTOs, and ports.
- `adapters/json/`: `FileAccess`/`JSON` parsing, save serialization, catalog repositories.
- `adapters/presenters/`: pure view-model builders that do not create Godot controls.
- `adapters/godot/`: implementations of ports backed by Godot APIs.
- `framework/godot_nodes/`: concrete `Node`, `Node2D`, `CanvasLayer`, and GDExtension classes.

## Boundary Inventory and Ownership

The `domain/` and `application/` trees are engine-neutral. They must not include
`godot_cpp`, name `godot::` types, or invoke Godot APIs. Engine values are
converted only in `adapters/` or `framework/` code.

Current boundary ownership:

- `MatchDirector` and `SpawnScheduler` use `std::string`, level definitions,
  ports, and match intents. `GameManager` converts their level labels and
  background paths to Godot `String` values when rendering the scene.
- `ContentValidator` consumes `ContentValidationInput`, a plain value model of
  menu, progression, upgrade, unit, and level data, and returns
  `std::vector<std::string>` issues. `JsonContentRepository` converts parsed
  Godot-backed catalog values into that input; `ContentStartupValidator` owns
  Godot logging.
- `GameManager` is the framework composition and lifecycle entry point. It
  delegates camera movement to `CameraScrollController`, backgrounds to
  `GameBackgroundBuilder`, node creation to `BaseObjectiveFactory` and
  `UnitFactory`, and match decisions to `MatchDirector`.
- `MenuManager` is the framework UI entry point. It delegates menu decisions
  and screen models to `MenuFlowUseCase` and presenter builders, style shaping
  to `MenuStyle`, and display/audio persistence to `SettingsService`.

Keep these translations at the edge. A new engine-facing value in domain or
application code is an architectural regression, not a convenience shortcut.


## Module 1: Match Runtime

The match runtime should own the core loop: start match, advance time, deploy units, account for deaths, update base integrity, detect victory/defeat, and build match-end outputs.

```mermaid
flowchart TB
    subgraph MatchDomain[Match Domain]
        MatchState[MatchState]
        MatchConfig[MatchConfig]
        ScoreRules[ScoreRules]
        EconomyRules[Energy and bounty rules]
        SpawnTimeline[SpawnTimeline]
    end

    subgraph MatchUseCases[Match Use Cases]
        StartMatch[StartMatch]
        AdvanceMatchTime[AdvanceMatchTime]
        RequestDeployment[RequestDeployment]
        ReportEnemyDefeated[ReportEnemyDefeated]
        ReportBaseIntegrity[ReportBaseIntegrity]
        FinishMatch[FinishMatch]
    end

    subgraph MatchPorts[Ports]
        UnitCatalogPort[UnitCatalog]
        SpawnPointPort[SpawnPointProvider]
        CampaignEffectsPort[CampaignEffects]
        MatchRewardPort[MatchRewardGateway]
    end

    subgraph MatchAdapters[Adapters]
        GameManagerAdapter[GameManager]
        GridManagerAdapter[GridManager]
        UnitDataRepository[Unit data repository]
        CampaignFacade[Campaign facade]
        UnitFactoryAdapter[UnitFactory]
    end

    GameManagerAdapter --> MatchUseCases
    MatchUseCases --> MatchDomain
    MatchUseCases --> MatchPorts
    GridManagerAdapter -. implements .-> SpawnPointPort
    UnitDataRepository -. implements .-> UnitCatalogPort
    CampaignFacade -. implements .-> CampaignEffectsPort
    CampaignFacade -. implements .-> MatchRewardPort
    GameManagerAdapter --> UnitFactoryAdapter
```

Current files that map into this module:

- `MatchSession` is the seed for `MatchState`, `EconomyRules`, and `ScoreRules`.
- `DeploymentService` is the seed for `RequestDeployment`.
- `SpawnScheduler` should split into a pure `SpawnTimeline` plus a use-case-owned clock advance.
- `MatchDirector` is currently a use-case coordinator, but should stop knowing concrete loaders and `Unit *`.
- `GameManager` should continue to apply `MatchUpdate`/intent outputs to Godot.

Match outputs are plain values:

- `SpawnUnitIntent { unit_id, side, position, runtime_profile }`
- `ResourceChanged { energy }`
- `WaveChanged { current_wave, total_waves }`
- `ScoreChanged { kill_score, total_score }`
- `MatchEnded { victory, summary_model, reward_options }`

Match use cases do not materialize `Unit` nodes. Godot adapters materialize spawn intents.

## Module 2: Combat and Entity Runtime

Pure logic produces combat decisions and commands, while Godot components adapt scene state and apply results.

```mermaid
flowchart TB
    subgraph CombatDomain[Combat Domain]
        CombatConfig[CombatConfig]
        CombatState[CombatState]
        TargetSnapshot[TargetSnapshot]
        TargetSelection[TargetSelection rules]
        CombatStep[Combat step rules]
        DamageRules[Damage and projectile rules]
    end

    subgraph CombatUseCases[Combat Use Cases]
        AdvanceCombat[AdvanceCombat]
        ResolveAttack[ResolveAttack]
        ResolveProjectile[ResolveProjectile]
    end

    subgraph CombatPorts[Ports]
        TargetSensorPort[TargetSensor]
        EntityCommandPort[EntityCommands]
        ProjectilePort[ProjectileSpawner]
        RandomPort[RandomSource]
    end

    subgraph CombatAdapters[Humble Godot Objects]
        CombatComponent[CombatComponent]
        DetectionComponent[DetectionComponent]
        MovementComponent[MovementComponent]
        AnimationController[AnimationController]
        HealthComponent[HealthComponent]
        ProjectileAttack[ProjectileAttack]
    end

    CombatComponent --> CombatUseCases
    CombatUseCases --> CombatDomain
    CombatUseCases --> CombatPorts
    DetectionComponent -. implements .-> TargetSensorPort
    MovementComponent -. implements .-> EntityCommandPort
    AnimationController -. implements .-> EntityCommandPort
    HealthComponent -. implements .-> EntityCommandPort
    ProjectileAttack -. implements .-> ProjectilePort
```

Target combat flow:

1. A Godot component collects target snapshots from areas and entity adapters.
2. A combat use case advances deterministic combat state.
3. The use case returns commands: stop, move, play pose, hide muzzle flash, deal damage, spawn projectile, play effect.
4. The Godot component applies commands to `MovementComponent`, `AnimationController`, `HealthComponent`, `ProjectileAttack`, and VFX/audio adapters.

## Module 3: Progression and Rewards

Progression is split across rules, use cases, repositories, presenters, and a Godot facade. `CampaignService` remains the script-facing singleton and composition root, while campaign behavior is coordinated through progression use cases and ports.

```mermaid
flowchart TB
    subgraph ProgressionDomain[Progression Domain]
        PlayerProfile[PlayerProfile]
        LevelUnlocks[LevelUnlock rules]
        UpgradeDefinitions[Upgrade definitions]
        UpgradeEffects[Upgrade effect rules]
        DraftRules[Draft selection rules]
        EffectiveStats[Effective stat calculation]
    end

    subgraph ProgressionUseCases[Progression Use Cases]
        LoadCampaign[LoadCampaign]
        SelectLevel[SelectLevel]
        CompleteLevel[CompleteLevel]
        BuildRewardDraft[BuildRewardDraft]
        ClaimUpgrade[ClaimUpgrade]
        BuildRoster[BuildAvailableRoster]
    end

    subgraph ProgressionPorts[Ports]
        ProfileRepository[ProfileRepository]
        ProgressionCatalogPort[ProgressionCatalog]
        UpgradeCatalogPort[UpgradeCatalog]
        RandomSource[RandomSource]
    end

    subgraph ProgressionAdapters[Adapters]
        CampaignServiceFacade[CampaignService Godot facade]
        ProgressionSaveJson[ProgressionSaveRepository]
        ProgressionCatalogJson[ProgressionCatalog JSON]
        UpgradeCatalogJson[UpgradeCatalog JSON]
        ProgressionPresenter[Progression presenter]
    end

    CampaignServiceFacade --> ProgressionUseCases
    ProgressionUseCases --> ProgressionDomain
    ProgressionUseCases --> ProgressionPorts
    ProgressionSaveJson -. implements .-> ProfileRepository
    ProgressionCatalogJson -. implements .-> ProgressionCatalogPort
    UpgradeCatalogJson -. implements .-> UpgradeCatalogPort
    ProgressionPresenter --> ProgressionDomain
```

Responsibilities:

- `PlayerProfile` is a plain save model.
- Upgrade eligibility, level unlocks, effective unit stats, and rescue draft thresholds live in deterministic progression rules.
- Draft selection accepts an injected `RandomSource` so tests can use seeded or scripted randomness.
- Persistence flows through a `ProfileRepository` port implemented by the JSON save adapter.
- `CampaignService` remains available to Godot as a singleton, but it delegates mutations and campaign orchestration to progression use cases rather than owning all rules directly.
- Presentation functions such as reward titles, subtitles, upgrade cards, owned-upgrade summaries, and level-select rows are pure presenters over progression outputs.

This split keeps singleton access, file I/O, random selection, rule calculation, and presentation shaping separated so progression behavior remains testable without Godot.

## Module 4: Content and Data Loading

Content loading lives at the adapter boundary: JSON repositories own file I/O and parsing, then hand use cases already-loaded content through plain models and catalog ports. Validation stays pure and reports issues for outer startup adapters to handle.

```mermaid
flowchart TB
    subgraph ContentFiles[Files]
        UnitJson[data/unit_data.json]
        GlobalsJson[data/unit_globals.json]
        LevelJson[data/levels/*.json]
        ProgressionJson[data/progression.json]
        UpgradesJson[data/upgrades.json]
        MenuJson[data/menu_data.json]
    end

    subgraph JsonAdapters[JSON Adapters]
        UnitParser[UnitDefinitionParser]
        LevelParser[LevelDefinitionParser]
        ProgressionParser[ProgressionCatalogParser]
        UpgradeParser[UpgradeCatalogParser]
        MenuParser[MenuContentParser]
        SaveParser[ProfileParser]
    end

    subgraph ContentDomain[Content Models]
        UnitDefinition[UnitDefinition]
        LevelDefinition[LevelDefinition]
        UpgradeCatalog[UpgradeCatalog]
        MenuDefinition[MenuDefinition]
        GameplayRules[GameplayRules]
    end

    subgraph ContentUseCases[Content Use Cases]
        ValidateContent[ValidateContent]
        BuildCatalogs[BuildRuntimeCatalogs]
    end

    ContentFiles --> JsonAdapters
    JsonAdapters --> ContentDomain
    ContentUseCases --> ContentDomain
```

## Module 5: Presentation, UI, and Scene Flow

Godot UI nodes are humble adapters: they render pure view models into controls and translate user input/signals into application intents. Presentation shaping, settings changes, and menu/post-match navigation decisions live in presenters and use cases; only Godot adapters touch `SceneTree`, display/audio APIs, and concrete engine services.

```mermaid
flowchart TB
    subgraph PresentationModels[Presentation Models]
        HudModel[HudModel]
        DeployCardModel[DeployCardModel]
        ScoreScreenModel[ScoreScreenModel]
        MenuScreenModel[MenuScreenModel]
        LevelSelectModel[LevelSelectModel]
        SettingsModel[SettingsModel]
    end

    subgraph Presenters[Pure Presenters]
        HudPresenter[HudPresenter]
        DeployCardPresenter[DeployCardPresenter]
        ScorePresenter[ScoreScreenPresenter]
        MenuPresenter[MenuPresenter]
        SettingsPresenter[SettingsPresenter]
    end

    subgraph UiAdapters[Humble UI Nodes]
        HUD[HUD CanvasLayer]
        MenuManager[MenuManager Node2D]
        PauseMenu[PauseMenu]
        GodotControls[Buttons, labels, panels]
    end

    subgraph ScenePorts[Scene and Settings Ports]
        SceneNavigationPort[SceneNavigation]
        SettingsPort[SettingsStore]
        AudioSettingsPort[AudioSettings]
    end

    Presenters --> PresentationModels
    UiAdapters --> Presenters
    UiAdapters --> ScenePorts
    HUD --> GodotControls
    MenuManager --> GodotControls
```

## Module 6: Godot Entity Construction

Entity construction stays outside the inner rules. Application outputs describe what should exist through intents, commands, runtime profiles, and resolved runtime configs; Godot factories decide how to call `memnew`, attach child nodes, configure components, and connect signals. `Unit`, `BaseObjective`, and `BattleEntity` remain humble Godot adapters for local setup, snapshots, signals, and command application, while inner rules interact through values, IDs, snapshots, commands, and intents rather than Godot node pointers.

```mermaid
flowchart TB
    subgraph EntityIntent[Inner Output]
        SpawnIntent[SpawnUnitIntent]
        EntityCommand[EntityCommand]
        VfxIntent[VfxIntent]
        AudioIntent[AudioIntent]
    end

    subgraph GodotFactories[Godot Factories]
        UnitFactory[UnitFactory]
        BaseObjectiveFactory[BaseObjectiveFactory]
        ProjectileFactory[ProjectileFactory]
        VfxFactory[Vfx factories]
    end

    subgraph GodotEntities[Godot Runtime Nodes]
        Unit[Unit]
        BaseObjective[BaseObjective]
        BattleEntity[BattleEntity]
        Components[Health, hitbox, detection, movement, animation, sound, combat]
    end

    SpawnIntent --> UnitFactory
    EntityCommand --> GodotEntities
    VfxIntent --> VfxFactory
    AudioIntent --> Components
    UnitFactory --> Unit
    BaseObjectiveFactory --> BaseObjective
    ProjectileFactory --> Components
    Unit --> BattleEntity
    BaseObjective --> BattleEntity
    BattleEntity --> Components
```

## Ports and Boundaries

Use ports only where they remove a real dependency. Avoid making an interface for every helper.

High-value ports:

| Port | Implemented by | Used by | Why |
| --- | --- | --- | --- |
| `UnitCatalog` | unit-data repository | deployment, spawn, roster use cases | Removes `UnitDataLoader` and JSON from use cases. |
| `LevelRepository` | level JSON repository | match start use case | Removes `LevelLoader` and file paths from match logic. |
| `ProfileRepository` | save JSON repository | progression use cases | Makes save/load testable. |
| `ProgressionCatalog` | progression JSON repository | progression rules | Keeps unlock data data-driven. |
| `UpgradeCatalog` | upgrades JSON repository | progression rules | Keeps upgrade definitions data-driven. |
| `SpawnPointProvider` | `GridManager` adapter | deployment and spawn use cases | Keeps Godot/world geometry at the edge. |
| `RandomSource` | Godot or standard RNG adapter | draft selection, resolved combat ranges | Enables deterministic tests. |
| `SceneNavigation` | `SceneNavigator` adapter | menu and post-match flow | Keeps `SceneTree` out of use cases. |
| `SettingsStore` | Godot display/audio adapter plus save file adapter | settings use case | Keeps OS/window/audio APIs at the edge. |

Low-value ports to avoid initially:

- wrappers around simple math functions;
- interfaces for every presenter;
- interfaces for one-off pure helpers;
- a dependency injection container.

Construct dependencies manually from scene roots or a small composition root.

## Testing Strategy

```mermaid
flowchart LR
    subgraph Native[Native tests]
        DomainTests[Domain rule tests]
        UseCaseTests[Use-case tests with fake ports]
        ParserTests[Parser tests from in-memory data]
        PresenterTests[Presenter/view-model tests]
    end

    subgraph Hosted[Godot-hosted tests]
        NodeWiring[Node wiring tests]
        SignalTests[Signal and lifecycle tests]
        ResourceTests[Resource loading smoke tests]
    end

    subgraph Manual[Manual or Playtest]
        Feel[Game feel]
        Visuals[Animation, VFX, audio]
    end

    DomainTests --> UseCaseTests
    ParserTests --> UseCaseTests
    PresenterTests --> Hosted
    Hosted --> Manual
```

Native tests should cover:

- scoring, energy, bounty, and victory/defeat rules;
- deployment affordability and spawn-intent generation;
- wave timeline advancement;
- combat target selection and attack timing;
- projectile/splash math;
- progression unlocks, upgrade effects, draft eligibility, and deterministic draft selection;
- content parser behavior from in-memory dictionaries or JSON text;
- presenter output models.

Hosted tests should cover:

- Godot node creation and component wiring;
- signal connections;
- `GameManager` and `MenuManager` adapter smoke tests;
- resource paths and scene registration;
- save path integration.

The native suite should be the default place for behavior. Hosted tests should prove that the humble objects are wired correctly.


## Practical Rules for Future Features

When adding a feature, use this checklist:

- Can the rule be tested without creating a Godot node? If not, move the rule inward.
- Does a use case return an intent or value model instead of mutating UI/nodes directly?
- Does a Godot class mostly translate lifecycle/signals/input into use-case calls and apply returned output?
- Are file I/O, resources, scene navigation, display settings, audio buses, and random numbers behind adapters or injected values?
- Is the test in the cheapest layer that can prove the behavior?

## Things Not to Do

- Do not make Godot-hosted tests the only way to test rules.
- Do not let `GameManager` or `CampaignService` accumulate new gameplay decisions.

## Desired End State

The desired end state is a project where the game can be understood in two passes:

1. Read domain and application code to understand how Defn plays, scores, progresses, deploys, spawns, and resolves combat.
2. Read Godot adapters to understand how those decisions appear on screen and how engine events enter the application.

That keeps the architecture simple: rules are plain and testable, Godot objects are humble, and data-driven content remains easy to change without hiding business logic inside scene code.
