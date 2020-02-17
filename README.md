# e3dcset


Mit diesem Linux Kommandozeilen Tool können einige Funktionen des S10 Hauskraftwerkes von E3DC über das RSCP Protokoll gesteuert werden. Dies sind:

- Ladeleistung des Speichers
- Entladeleistung des Speichers
- Zurückschalten von Lade-/Entladeleistung auf Automatik
- Sofortiges Laden des Speichers mit einer bestimmten Energiemenge starten

# Motivation

Bezug von Reststrom preisoptimal gestalten. Details dazu sind [hier] in meinem Blogpost zu finden.

# Installation

Zunächst das git Repository klonen mit

```sh
$ git clone https://github.com/mschlappa/e3dcset.git
```
In das soeben angelegte Verzeichnis e3dcset wechseln und die Datei e3dcset.cpp mit einem Editor Deiner Wahl öffnen (z.B. nano)

```sh
$ cd e3dcset
$ nano e3dcset.cpp
```

- IP-Adresse sowie Port (Standard 5033) des Hauskraftwerkes anpassen.
- Username / Kennwort vom E3DC Online Account sowie das im Gerät zuvor selbst festgelegte RSCP-Kennwort eintragen 
- Datei beim Verlassen speichern ;-)

```sh
#define SERVER_IP           "192.168.xxx.yyy"
#define SERVER_PORT         5033

#define E3DC_USER           "deine@email.de"
#define E3DC_PASSWORD       "passwort e3dc online account"
#define AES_PASSWORD        "passwort s10 rscp"
```

Kompilieren des Tools mit

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

Ladeleistung 2000 Watt / Entladen des Speichern unterbinden mit 

```sh
$ ./e3dcset -c 2000 -d 1

Connecting to server 192.168.1.42:5033
Connected successfully
Request authentication
RSCP authentitication level 10
Setze maxLadeLeistung=2000W maxEntladeLeistung=1W
Done!
```

Ladeleistung / Entladeleistung zurück auf Automatik stellen:

```sh
$ ./e3dcset -a

Connecting to server 192.168.1.42:5033
Connected successfully
Request authentication
RSCP authentitication level 10
Setze automatischLeistungEinstellen aktiv
Done!
```

Speicher mit 1 kWh laden. 
Ladeleistung soll 2400 Watt betragen und das Entladen des Speichers soll unterbunden werden

```sh
./e3dcset -c 2400 -d 1 -e 1000
Connecting to server 192.168.1.42:5033
Connected successfully
Request authentication
RSCP authentitication level 10
Setze maxLadeLeistung=2400W maxEntladeLeistung=1W
MANUAL_CHARGE_STARTED
Tag 0x0180008F received error code 7.
Done!
```

Hinweise: 
- Wenn nicht genug PV Leistung zur Verfügung steht, wird der Strom aus dem Netz geladen.
- Die Fehlermeldung ```Tag 0x0180008F received error code 7.``` ist ein bekannter Fehler meines Tools. Ich habe bisher noch keine Lösung dafür gefunden. Die Funktion "Speicher laden" funktioniert jedoch ohne Probleme.


[//]: # (These are reference links used in the body of this note and get stripped out when the markdown processor does its job. There is no need to format nicely because it shouldn't be seen. Thanks SO - http://stackoverflow.com/questions/4823468/store-comments-in-markdown-syntax)


   [hier]: <https://elektromobilitaet-duelmen.de/2019/11/22/winter-is-coming/>
   
   
