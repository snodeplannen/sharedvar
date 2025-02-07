#pragma once

// De volgende macro's definiëren de minimale Windows-platformversie die vereist is.
// De minimale vereiste is de vroegste Windows-versie die de benodigde features bevat om de applicatie uit te voeren.
// Alle features die specifiek zijn voor een bepaalde Windows-versie worden automatisch ingeschakeld.

#include <winsdkver.h>

#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00 