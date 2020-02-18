# e3dcset


Mit diesem Linux Kommandozeilen Tool können einige Funktionen des S10 Hauskraftwerkes von E3DC über das RSCP Protokoll gesteuert werden. Dies sind:

- Ladeleistung des Speichers
- Entladeleistung des Speichers
- Zurückschalten von Lade-/Entladeleistung auf Automatik
- Sofortiges Laden des Speichers mit einer bestimmten Energiemenge (ggf. auch mit Netzstrom)

Das Programm basiert auf dem von E3DC zur Verfügung gestellten Beispielprogramm sowie 
einigen Codestellen aus dem Tool [E3DC-Control] von Eberhard Meyer.

Dieses Tool setzt den übergebenen Befehl an das Hauskraftwerk ab und beendet sich dann gleich wieder.  

# Motivation

Bezug von Reststrom preisoptimal gestalten. Details dazu sind [hier] in meinem Blogpost zu finden.

# Installation

Zunächst das git Repository klonen mit:

```sh
$ git clone https://github.com/mschlappa/e3dcset.git
```
In das soeben angelegte Verzeichnis e3dcset wechseln und die Konfigurationsdatei Datei e3dcset.config mit einem Editor Deiner Wahl öffnen (z.B. nano)

```sh
$ cd e3dcset
$ nano e3dcset.config
```

- IP-Adresse sowie Port (Standardport=5033) auf die des eigenen Hauskraftwerkes anpassen.
- Username / Kennwort vom E3DC Online Account sowie das im Gerät zuvor selbst festgelegte RSCP-Kennwort eintragen 
- ggf. debug auf 1 setzen, falls zusaetzliche Ausgaben (z.B zur Fehlersuche) gewuenscht werden 
- Datei beim Verlassen speichern ;-)

```sh
server_ip = 127.0.0.1
server_port = 5033
e3dc_user = xxx@xxxxx.xx
e3dc_password = xxxxx
aes_password = xxxxx
```

Kompilieren des Tools mit:

```sh
$ make
```
Hinweis: Das kann auf einem älteren Raspberry Pi ein paar Minuten dauern ...

# Aufrufbeispiele

Nachdem das Kompilieren angeschlossen ist, kann man das Tool ohne Parameter aufrufen.

Es wird dann eine kleine Hilfe ausgegeben:

```sh
$ ./e3dcset

Usage: e3dcset [-c maxLadeLeistung] [-d maxEntladeLeistung] [-e manuelleLadeEnergie] [-a]
```

Ladeleistung 2000 Watt / Entladen des Speichern unterbinden mit:

```sh
$ ./e3dcset -c 2000 -d 1

Setze maxLadeLeistung=2000W maxEntladeLeistung=1W

```

Ladeleistung / Entladeleistung zurück auf Automatik stellen:

```sh
$ ./e3dcset -a

Setze automatischLeistungEinstellen aktiv

```

Speicher mit 1 kWh (1000 Wh) laden. 
Ladeleistung soll 2400 Watt betragen und das Entladen des Speichers soll unterbunden werden:

```sh
./e3dcset -c 2400 -d 1 -e 1000

Setze maxLadeLeistung=2400W maxEntladeLeistung=1W
Tag 0x0180008F received error code 7.

```

Hinweise: 
- Wenn nicht genug PV Leistung zur Verfügung steht, wird der Strom aus dem Netz geladen.
- Die Fehlermeldung ```Tag 0x0180008F received error code 7.``` ist ein bekannter Fehler meines Tools. Ich habe bisher noch keine Lösung dafür gefunden. Die Funktion "Speicher laden" funktioniert jedoch ohne Probleme.

Viel Spaß beim Ausprobieren!


[hier]: https://elektromobilitaet-duelmen.de/2019/11/22/winter-is-coming/   
[E3DC-Control]: https://github.com/Eba-M/E3DC-Control/  
 
