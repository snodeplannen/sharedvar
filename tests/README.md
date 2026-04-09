# Tests — Cross-Language Integratietests

Integratietests die de COM Server valideren vanuit verschillende programmeertalen en scenario's.

## Rol binnen het project

Deze tests bevinden zich in de **project root** en zijn primair gericht op de **Legacy DLL** variant (`ATLProjectcomserver.SharedValue` / `ATLProjectcomserver.DatasetProxy`). Voor de EXE-variant bestaan aparte tests in [`ATLProjectcomserverExe/tests/`](../ATLProjectcomserverExe/tests/).

## Directorystructuur

```
tests/
├── CSharpNet8Test/             # C# .NET 8 integratietests (DLL variant)
│   ├── Program.cs              # Testprogramma (CRUD, error handling, storage modes)
│   └── CSharpNet8Test.csproj   # .NET 8 projectbestand
└── VBScriptTest/               # VBScript integratietests
    ├── TestProducerSleep.vbs   # Producer die data schrijft en wacht
    ├── TestProducerString.vbs  # Producer die een string wegschrijft
    ├── TestProducerVBS.vbs     # Producer met DatasetProxy bewerkingen
    ├── TestConsumerString.vbs  # Consumer die een string uitleest
    ├── TestConsumerVBS.vbs     # Consumer die DatasetProxy rijen uitleest
    ├── TestFullCycle.vbs       # Volledige producer+consumer cyclus in één script
    ├── TestErrorHandling.vbs   # Test van COM error handling (KeyNotFound, etc.)
    └── TestEventCallbacks.vbs  # Test van observer event callbacks
```

## Tests Draaien

### C# .NET 8

```powershell
cd tests\CSharpNet8Test
dotnet run
```

### VBScript

```powershell
cscript //Nologo tests\VBScriptTest\TestFullCycle.vbs
```

## Vereisten

- De Legacy DLL COM Server moet geregistreerd zijn via `regsvr32`.
- .NET 8 SDK voor de C# tests.
- `cscript.exe` (standaard aanwezig op Windows) voor VBScript tests.
