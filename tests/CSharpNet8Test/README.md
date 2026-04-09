# CSharpNet8Test — C# .NET 8 DatasetProxy Integratietests (Legacy DLL)

Een C# console-applicatie die de **Legacy DLL** COM Server (`ATLProjectcomserver.DatasetProxy`) end-to-end test.

## Rol binnen het project

Deze variant test de **InprocServer32 DLL** (in tegenstelling tot de [EXE-variant tests](../../ATLProjectcomserverExe/tests/CSharpNet8Test/)). Dezelfde testcategorieën worden doorlopen.

## Bestandsoverzicht

| Bestand | Beschrijving |
|---|---|
| `Program.cs` | Testprogramma met 5 categorieën: Producer, Consumer, Mutatie, Error Handling, Storage Mode. |
| `CSharpNet8Test.csproj` | .NET 8 projectbestand. |

## Draaien

```powershell
cd tests\CSharpNet8Test
dotnet run
```

## Vereisten

- .NET 8 SDK.
- Legacy DLL moet geregistreerd zijn (`regsvr32 x64\Debug\ATLProjectcomserver.dll`).
