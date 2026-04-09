# CSharpNet8Test — C# .NET 8 DatasetProxy Integratietests

Een C# console-applicatie die de `DatasetProxy` COM component end-to-end test op correctheid van CRUD-operaties, foutafhandeling, paginering en storage mode configuratie.

## Rol binnen het project

Deze tests valideren de cross-language interoperabiliteit tussen de C++ COM Server en .NET 8 clients via late binding (`dynamic` / `Activator.CreateInstance`). Ze dienen als regressie-suite voor de `IDatasetProxy` interface.

## Bestandsoverzicht

| Bestand | Beschrijving |
|---|---|
| `Program.cs` | Hoofd testprogramma met 5 testcategorieën: Producer Flow, Consumer Flow, Bidirectionele Mutatie, Error Handling, en Storage Mode. |
| `CSharpNet8Test.csproj` | .NET 8 projectbestand. |

## Testcategorieën

1. **Producer Flow** — Schrijft 100 rijen via `AddRow` en parkeert de proxy in `SharedValue`.
2. **Consumer Flow** — Leest de rijen terug, valideert data-integriteit en paginering via `FetchPageKeys`.
3. **Bidirectionele Mutatie** — Test `UpdateRow` en `RemoveRow` operaties.
4. **Error Handling** — Valideert `COMException` bij `KeyNotFound`, `DuplicateKey` en `StoreModeNotEmpty`.
5. **Storage Mode** — Schakelt naar `Unordered` mode en verifieert CRUD-functionaliteit.

## Draaien

```powershell
cd ATLProjectcomserverExe\tests\CSharpNet8Test
dotnet run
```

## Vereisten

- .NET 8 SDK.
- De COM Server (DLL of EXE) moet geregistreerd zijn.
