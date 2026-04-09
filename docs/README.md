# Docs — Ontwerpdocumentatie

Achtergrondanalyses en ontwerpdocumenten die de architecturale beslissingen van dit project onderbouwen.

## Rol binnen het project

Deze documenten dienen als historische referentie voor de technische keuzes die zijn gemaakt tijdens de ontwikkeling, met name de migratie van in-process DLL naar out-of-process EXE.

## Bestandsoverzicht

| Bestand | Beschrijving |
|---|---|
| `3_methods_for_inteprocess.md` | **Cross-Process Architecture Redesign** — Analyse van drie migratiestrategieën voor cross-process data-sharing: (1) Native Out-of-Process EXE (`LocalServer32`), (2) Windows DLL Surrogate (`dllhost.exe`), en (3) Shared Memory (`CreateFileMapping`). Bevat voor- en nadelen per optie en het uiteindelijke advies. |

## Context

Dit document werd opgesteld toen het project nog uitsluitend als In-Process DLL opereerde. Het beschrijft de afweging die leidde tot de keuze voor **Optie 1 (ATL EXE)**, die vervolgens is geïmplementeerd in de [`ATLProjectcomserverExe/`](../ATLProjectcomserverExe/) directory.
