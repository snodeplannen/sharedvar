# ATL COM Server Project

Dit project is een ATL COM server met de volgende componenten:

1. MathOperations - Een component voor basis rekenkundige operaties
2. SharedValue - Een component voor het delen van waarden tussen clients
3. SharedValueCallback - Een callback interface voor waardewijzigingen

## Vereisten

- Visual Studio 2022 met ATL ondersteuning
- Windows 10 SDK of nieuwer
- Administrator rechten voor COM registratie

## Bouwen

1. Open `ATLProjectcomserver.sln` in Visual Studio
2. Selecteer de gewenste configuratie (Debug/Release) en platform (x86/x64)
3. Build de solution (F7)

## Registreren

De COM server wordt automatisch geregistreerd tijdens het bouwen als Visual Studio met administrator rechten draait.
Anders kun je de volgende commando's gebruiken (als administrator):

```cmd
regsvr32 ATLProjectcomserver.dll   # Voor registratie
regsvr32 /u ATLProjectcomserver.dll # Voor de-registratie
```

## Gebruik

De COM server biedt de volgende interfaces:

### IMathOperations
- Add(double a, double b) -> double
- Subtract(double a, double b) -> double
- Multiply(double a, double b) -> double
- Divide(double a, double b) -> double

### ISharedValue
- SetValue(VARIANT value)
- GetValue() -> VARIANT
- GetCurrentUTCDateTime() -> BSTR
- GetCurrentUserLogin() -> BSTR
- Lock(long timeoutMs)
- Unlock()
- Subscribe(ISharedValueCallback* callback)
- Unsubscribe(ISharedValueCallback* callback)

### ISharedValueCallback
- OnValueChanged(VARIANT newValue)
- OnDateTimeChanged(BSTR newDateTime)

## ProgIDs

- ATLProjectcomserver.MathOperations
- ATLProjectcomserver.SharedValue
- ATLProjectcomserver.SharedValueCallback 