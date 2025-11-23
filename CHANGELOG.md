# Changelog

Alle bedeutenden Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

## [Unreleased] - 2025-01-23

### Hinzugefügt
- **RSCP Tags für Multiple-DCB-Abfrage**: TAG_BAT_REQ_DCB_ALL_CELL_TEMPERATURES (0x03000018) und TAG_BAT_REQ_DCB_ALL_CELL_VOLTAGES (0x0300001A) in RscpTags.h hinzugefügt
- Diese Tags werden im Modul-Info-Dump (-m 0) zusätzlich angefordert

### Geändert  
- Request für Modul-Info-Dump erweitert um TAG_BAT_REQ_DCB_ALL_CELL_TEMPERATURES und TAG_BAT_REQ_DCB_ALL_CELL_VOLTAGES
- Basierend auf rscpgui Analyse: Diese Tags könnten ALLE DCB-Module zurückgeben

### Erkenntnisse aus python-e3dc Library Analyse
Die zugrundeliegende Python-Library (fsantini/python-e3dc), die von RSCPGui verwendet wird, macht **mehrere separate Requests**:
1. Erst TAG_BAT_REQ_DCB_COUNT abfragen → erhält Anzahl DCBs
2. Dann für JEDEN DCB-Index (0, 1, 2...) einen separaten Request mit:
   - BAT_REQ_DATA Container
   - BAT_INDEX = 0
   - DCB_INDEX = (aktueller DCB)
   - BAT_REQ_DCB_INFO

**Beispiel aus python-e3dc:**
```python
if "dcbs" in battery:
    dcbs = list(range(0, battery["dcbs"]))  # [0, 1] für 2 DCBs
for dcbIndex in dcbs:
    # Separater Request für jeden DCB
    get_battery_data(batIndex=0, dcbs=[dcbIndex])
```

### Nächste Schritte
1. **Test der aktuellen Implementierung**: Die neuen Tags (DCB_ALL_CELL_*) könnten trotzdem funktionieren
2. **Falls nicht**: Code umschreiben um mehrere Requests zu machen (wie python-e3dc)
   - Erst DCB_COUNT abfragen
   - Dann Schleife über jeden DCB-Index
   - Für jeden DCB separaten Request senden

## [Previous] - 2025-01-22

### Hinzugefügt
- DCB-spezifische Abfragen mit TAG_BAT_REQ_DCB_INFO (0x03000042)
- Vollständige Darstellung von DCB #0 mit 35 Datenpunkten
- Debug-Modus mit Credential-Maskierung
- Container-Parser für TAG_BAT_DCB_INFO mit Gruppierung nach DCB_INDEX
