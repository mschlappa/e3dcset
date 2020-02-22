# e3dcset


Mit diesem Linux Kommandozeilen Tool können einige Funktionen des S10 Hauskraftwerkes von E3DC über das RSCP Protokoll gesteuert werden. Dies sind:

- Lade-/Entladeleistung des Speichers ändern
- Zurückschalten der Lade-/Entladeleistung auf Automatik
- Manuelle Speicherladung mit einer bestimmten Energiemenge starten (ggf. auch mit Netzstrom)
- Laufende manuelle Speicherladung stoppen

Das Programm basiert auf dem von E3DC zur Verfügung gestellten Beispielprogramm sowie 
einigen Codestellen aus dem Tool [E3DC-Control] von Eberhard Mayer.

Dieses Tool setzt die übergebenen Kommandos an das Hauskraftwerk ab und beendet sich danach.  

# Motivation

Die Grundidee bestand darin, den im Winter unausweichlichen (Rest-)Strombezug in die Zeiten zu verlagern, in denen er besonders günstig ist. 

Idealerweise sollte zu diesen Zeiten auch ein Überangebot an Strom aus erneuerbaren Energien vorhanden sein, damit neben dem ökonomischen auch der ökologische Aspekt nicht zu kurz kommt. Stürmisches Herbstwetter kann in diesem Zusammenhang sehr hilfreich sein.

 Details dazu sind [hier] in meinem Blogpost zu finden.

# Installation

Zunächst das git Repository klonen mit:

```sh
$ git clone https://github.com/mschlappa/e3dcset.git
```
In das soeben angelegte Verzeichnis ``e3dcset`` wechseln und die Konfigurationsdatei Datei ``e3dcset.config`` mit einem Editor Deiner Wahl öffnen (z.B. nano)

```sh
$ cd e3dcset
$ nano e3dcset.config
```

- IP-Adresse sowie Port (Standardport=5033) auf die des eigenen Hauskraftwerkes anpassen.
- Username / Kennwort vom E3DC Online Account sowie das im Gerät zuvor selbst festgelegte RSCP-Kennwort eintragen 
- ggf. debug auf 1 setzen, falls zusaetzliche Ausgaben (z.B zur Fehlersuche) gewuenscht werden
- Datei beim Verlassen speichern ;-)

```sh
server_ip = 192.168.xxx.yyy
server_port = 5033
e3dc_user = xxx@xxxxx.xx
e3dc_password = xxxxx
aes_password = xxxxx
```
Die Min/Max Werte sind bereits auf sinnvolle Werte eingestellt und müssen i.d.R. nicht angepasst werden (Ausnahme: z.B. S10 Pro wg. erhöhter Lade-/Entladeleistung)


Kompilieren des Tools mit:

```sh
$ make
```
Hinweis: Das kann auf einem älteren Raspberry Pi ein paar Minuten dauern ...

# Aufrufbeispiele

Nachdem das Kompilieren angeschlossen ist, kann man das Tool ohne Parameter aufrufen.

Es wird dann eine kleine Hilfe ausgegeben.

Bedeutung der Kürzel:
-(c)harge
-(d)ischarge
-(e)nergy
-(a)utomatic
-(p)ath

```sh
$ ./e3dcset

Usage: e3dcset [-c LadeLeistung] [-d EntladeLeistung] [-e LadungsMenge] [-a] [-p Pfad zur Konfigurationsdatei]
```

Ladeleistung 2000 Watt / Entladen des Speichers unterbinden mit:

```sh
$ ./e3dcset -c 2000 -d 1
Setze LadeLeistung auf 2000W 
Setze EntladeLeistung auf 1W
```

Ladeleistung / Entladeleistung zurück auf Automatik stellen und den Pfad zur Konfigurationsdatei angeben (normalerweise wird im aktuellen Pfad nach der Datei e3dcset.config gesucht):

```sh
$ ./e3dcset -a -p /home/pi/meine.config
```
Ladeleistung / Entladeleistung zurück auf Automatik stellen und laufende manuelle Speicherladung stoppen:

```sh
$ ./e3dcset -a -e 0
Setze LadeLeistung auf Automatik
Manuelles Laden gestoppt
```

Manuelles Laden des Speichers mit 1 kWh (1000 Wh) starten. 
Ladeleistung soll 2400 Watt betragen und das Entladen des Speichers soll unterbunden werden:

```sh
./e3dcset -c 2400 -d 1 -e 1000
Setze LadeLeistung auf 2400W 
Setze EntladeLeistung auf 1W
Manuelles Laden gestartet
```

Hinweise: 

- Wenn nicht genug PV Leistung zur Verfügung steht, wird beim manuelles Laden des Speichers der Strom aus dem Netz bezogen.


Viel Spaß beim Ausprobieren!


[hier]: https://elektromobilitaet-duelmen.de/2019/11/22/winter-is-coming/   
[E3DC-Control]: https://github.com/Eba-M/E3DC-Control/  
 
