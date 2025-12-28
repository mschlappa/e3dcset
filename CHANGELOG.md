# Changelog

Alle bedeutenden Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

## [Unreleased] - 2025-12-28

### Hinzugefügt
- **Parameter -E (Notstromreserve setzen)**: Neuer Parameter zum Setzen der Notstromreserve in Wh
  - `./e3dcset -E 2600` - Setzt Reserve auf 2600 Wh
  - `./e3dcset -E 0` - Deaktiviert die Reserve
  - **Workaround für Netzladung**: Da -e (manuelles Laden) nur bedingt funktioniert, kann über die Notstromreserve eine Netzladung erzwungen werden
  - Neue RSCP-Tags: TAG_EP_REQ_SET_EP_RESERVE, TAG_EP_PARAM_INDEX, TAG_EP_PARAM_EP_RESERVE_ENERGY
- **Multi-DCB Unterstützung**: Vollständige Anzeige ALLER DCB-Module (Zellblöcke) eines E3DC S10 Systems
- **Multi-Request-Architektur**: Implementiert sequenzielle Abfragen basierend auf rscp2mqtt C++ Implementierung
  1. Erste Anfrage: Batterie-Daten + TAG_BAT_REQ_DCB_COUNT → ermittelt Anzahl DCBs
  2. Folge-Anfragen: TAG_BAT_REQ_DCB_INFO mit DCB-Index als Wert → liefert Daten für jeden DCB
- **CommandContext-Erweiterung**: DCB-Tracking mit `totalDCBs`, `currentDCBIndex`, `needMoreDCBRequests`, `isFirstModuleDumpRequest`
- **Automatische Loop-Verwaltung**: mainLoop läuft automatisch weiter bis alle DCBs abgefragt wurden
- **RSCP Tags für DCB-Abfrage**: TAG_BAT_REQ_DCB_ALL_CELL_TEMPERATURES (0x03000018) und TAG_BAT_REQ_DCB_ALL_CELL_VOLTAGES (0x0300001A) in RscpTags.h hinzugefügt

### Geändert  
- **Request-Building-Logik** in createRequestExample():
  - Erster Request: Battery-Level-Tags + TAG_BAT_REQ_DCB_COUNT
  - Weitere Requests: Nur TAG_BAT_REQ_DCB_INFO mit Index-Wert (korrekte RSCP-Syntax)
- **Response-Verarbeitung** in handleResponseValue():
  - TAG_BAT_DATA: Extrahiert DCB_COUNT und initialisiert Multi-Request-Loop
  - TAG_BAT_DCB_INFO: Zeigt DCB-Daten an, inkrementiert Index, beendet Loop nach letztem DCB
- **mainLoop**: Prüft `needMoreDCBRequests` und läuft weiter oder stoppt entsprechend

### Behoben
- **Kritischer Bug**: TAG_BAT_DCB_INDEX (0x03800100) ist ein RESPONSE-Tag und darf NICHT im Request verwendet werden
  - **Korrekte Lösung**: TAG_BAT_REQ_DCB_INFO nimmt den DCB-Index als Wert: `appendValue(&container, TAG_BAT_REQ_DCB_INFO, (uint8_t)dcbIndex)`
- **Loop-Terminierung**: Inkrementierung und Terminierungsprüfung erfolgen jetzt konsistent in TAG_BAT_DCB_INFO Response-Handler
- **State-Reset**: `isFirstModuleDumpRequest` wird auf `true` zurückgesetzt nach vollständigem DCB-Durchlauf → wiederholte Ausführungen funktionieren
- **Memory-Management**: DCB-Daten werden sofort ausgegeben (keine Akkumulation), SRscpValue-Container korrekt mit destroyValueData freigegeben

### Technische Details
Die Implementierung basiert auf Analyse von:
- **rscp2mqtt** (pvtom/rscp2mqtt): C++ Referenzimplementierung für Multi-DCB-Requests
- **python-e3dc** (fsantini/python-e3dc): Python-Library mit DCB-Iteration

**Korrekte RSCP-Syntax für DCB-Abfragen:**
```cpp
// FALSCH (Error 7):
protocol.appendValue(&container, TAG_BAT_DCB_INDEX, dcbIndex);
protocol.appendValue(&container, TAG_BAT_REQ_DCB_INFO);

// RICHTIG:
protocol.appendValue(&container, TAG_BAT_REQ_DCB_INFO, (uint8_t)dcbIndex);
```

**Beispiel-Ausgabe (System mit 2 DCBs):**
```
Batterie Modul 0:
  Relativer Ladezustand (RSOC):    85.00 %
  ...
  Anzahl DCB-Module:               2

  === DCB Zellblöcke ===
  Zellblock 0:
    State of Health (SOH):         48.90 %
    Ladezyklen:                    1774
    Strom:                         -2.50 A
    ...

  Zellblock 1:
    State of Health (SOH):         84.90 %
    Ladezyklen:                    1557
    Strom:                         -2.48 A
    ...
```

## [Previous] 

### Hinzugefügt
- DCB-spezifische Abfragen mit TAG_BAT_REQ_DCB_INFO (0x03000042)
- Vollständige Darstellung von DCB #0 mit 35 Datenpunkten
- Debug-Modus mit Credential-Maskierung
- Container-Parser für TAG_BAT_DCB_INFO mit Gruppierung nach DCB_INDEX
