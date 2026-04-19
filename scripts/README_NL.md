# Scripts — Hulpmiddelen & Diagnostiek

PowerShell scripts voor het diagnosticeren en beheren van de COM Server registraties.

## Rol binnen het project

Deze scripts automatiseren het verifiëren van de Windows Registry instellingen die nodig zijn voor correcte COM-communicatie. Ze zijn essentieel voor het debuggen van registratieproblemen na een build of herinstallatie.

## Bestandsoverzicht

| Bestand | Beschrijving |
|---|---|
| `Invoke-ComDiagnostics.ps1` | **COM diagnostisch script.** Controleert voor zowel de DLL (`InprocServer32`) als de EXE (`LocalServer32`) variant: CLSID/ProgID registraties, TypeLib, interface proxy/stub registraties, en voert een runtime laadtest uit. |

## Gebruik

```powershell
.\scripts\Invoke-ComDiagnostics.ps1
```

### Wat wordt er gecontroleerd?

Per servervariant (DLL en EXE):

1. **Klasse Registraties** — ProgID en CLSID voor `SharedValue`, `DatasetProxy`, `MathOperations`.
2. **TypeLib Registratie** — Aanwezigheid van de type library in het register.
3. **Interface & Proxy/Stub** — `ISharedValue`, `IDatasetProxy`, `IEventCallback` met hun ProxyStubClsid32.
4. **Runtime Laadtest** — Instantieert `SharedValue` via COM en roept `GetValue()` aan ter verificatie.

De output toont per check `[OK]` of `[FAIL]` met details.


## Gerelateerde Documentatie

- [README.md](../README.md) — Hoofddocumentatie en startpunt van het gehele project.
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Hoofd architectuurdocument voor het gehele COM Server project.
- [README.md](../ATLProjectcomserverExe/README.md) — Gebruikershandleiding en overzicht van de EXE COM Server variant.
- [README.md](../SharedValueV2/README.md) — Introductie en overzicht van de SharedValueV2 C++20 engine.
