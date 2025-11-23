# Changelog

Alle bedeutenden Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

## [Unreleased] - 2025-01-23

### Hinzugefügt
- **RSCP Tags für Multiple-DCB-Abfrage**: TAG_BAT_REQ_DCB_ALL_CELL_TEMPERATURES (0x03000018) und TAG_BAT_REQ_DCB_ALL_CELL_VOLTAGES (0x0300001A) hinzugefügt
- Diese Tags werden im Modul-Info-Dump (-m 0) verwendet und sollten ALLE DCB-Module zurückgeben (nicht nur DCB #0)

### Geändert  
- Request für Modul-Info-Dump erweitert um TAG_BAT_REQ_DCB_ALL_CELL_TEMPERATURES und TAG_BAT_REQ_DCB_ALL_CELL_VOLTAGES
- Dies entspricht der Implementierung in rscpgui und sollte beide DCB-Module (#0 und #1) zurückliefern

### Bekannte Probleme
- Response-Verarbeitung muss möglicherweise noch angepasst werden, falls E3DC mehrere separate DCB_INFO Container zurückgibt
- Testergebnisse vom echten E3DC-System ausstehend

## [Previous] - 2025-01-22

### Hinzugefügt
- DCB-spezifische Abfragen mit TAG_BAT_REQ_DCB_INFO (0x03000042)
- Vollständige Darstellung von DCB #0 mit 35 Datenpunkten
- Debug-Modus mit Credential-Maskierung
- Container-Parser für TAG_BAT_DCB_INFO mit Gruppierung nach DCB_INDEX
