# BUILDING – Build-Anleitung

Vollständige Anleitung zum Kompilieren, Testen und Installieren von `e3dcset`.

---

## Inhaltsverzeichnis

- [Voraussetzungen](#voraussetzungen)
- [Schnellstart](#schnellstart)
- [Build-Optionen](#build-optionen)
- [Tests](#tests)
- [Installation](#installation)
- [Plattform-spezifisch](#plattform-spezifisch)
- [Fehlerbehebung](#fehlerbehebung)

---

## Voraussetzungen

### Compiler & Build-Tools

**Erforderlich:**
- **g++** (GCC C++ Compiler) – Version 4.8+ (C++11-Support)
- **make** – GNU Make
- **Standard C/C++ Libraries** – libstdc++, glibc

**Installation:**

**Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install build-essential
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc-c++ make
```

**Arch Linux:**
```bash
sudo pacman -S base-devel
```

**macOS:**
```bash
# Xcode Command Line Tools
xcode-select --install

# Oder via Homebrew
brew install gcc make
```

---

### OpenSSL (Optional)

**Hinweis:** `e3dcset` nutzt eine **eigene AES-Implementierung** (AES.cpp), OpenSSL ist **nicht erforderlich**.

Falls Sie OpenSSL trotzdem installieren möchten:
```bash
# Debian/Ubuntu
sudo apt install libssl-dev

# macOS
brew install openssl
```

---

### Abhängigkeiten prüfen

```bash
# Compiler-Version prüfen
g++ --version
# Sollte ≥ 4.8 sein (für C++11)

# Make prüfen
make --version
```

---

## Schnellstart

### 1. Repository klonen

```bash
git clone https://github.com/jarvis-schlappa/e3dcset.git
cd e3dcset
```

---

### 2. Kompilieren

```bash
make
```

**Ausgabe:**
```
g++ -std=c++11 -Wall -I./include -c src/e3dcset.cpp -o build/e3dcset.o
g++ -std=c++11 -Wall -I./include -c src/config.cpp -o build/config.o
g++ -std=c++11 -Wall -I./include -c src/rscp_handler.cpp -o build/rscp_handler.o
...
g++ -o e3dcset build/*.o
Build complete: e3dcset
```

---

### 3. Testen

```bash
# Binary ausführbar?
./e3dcset --help

# Unit-Tests
make test
```

---

### 4. Konfigurieren

```bash
# Config erstellen
cp e3dcset.config.example e3dcset.config
nano e3dcset.config

# Berechtigungen setzen
chmod 600 e3dcset.config
```

---

### 5. Erste Abfrage

```bash
./e3dcset -r EMS_BAT_SOC
```

---

## Build-Optionen

### Standard-Build

```bash
make
```

Kompiliert alle Source-Dateien und erstellt `e3dcset` Binary.

---

### Clean Build

```bash
# Alle Build-Artefakte löschen
make clean

# Neu kompilieren
make
```

**Entfernt:**
- `build/*.o` (Object-Dateien)
- `build/*.d` (Dependency-Dateien)
- `e3dcset` (Binary)
- `tests/test_*` (Test-Binaries)

---

### Debug-Build

```bash
# Mit Debug-Symbolen
make CXXFLAGS="-std=c++11 -Wall -g -O0"
```

**Nutzen:**
- Debugging mit `gdb`
- Bessere Fehlermeldungen
- Keine Optimierung (langsamer)

**Verwendung:**
```bash
gdb ./e3dcset
(gdb) run -r EMS_BAT_SOC
```

---

### Optimierter Build

```bash
# Maximale Optimierung
make CXXFLAGS="-std=c++11 -Wall -O3"
```

**Optimierungslevel:**
- `-O0` – Keine Optimierung (Standard für Debug)
- `-O1` – Basis-Optimierung
- `-O2` – Standard-Optimierung (empfohlen)
- `-O3` – Maximale Optimierung (schnellster Code)

---

### Statisches Linking

```bash
# Alle Libraries statisch linken
make LDFLAGS="-static"
```

**Nutzen:**
- Binary läuft ohne externe Libraries
- Ideal für Distribution

**Nachteil:**
- Größere Binary-Größe

---

## Tests

### Unit-Tests ausführen

```bash
make test
```

**Ausgabe:**
```
=========================================
Running e3dcset Test Suite
=========================================

=== Testing Config Parsing ===
  Running: config_parse_integer... PASS
  Running: config_parse_string... PASS
  Running: config_validate_required... PASS
  ...

=== Testing String Handling ===
  Running: safe_string_copy... PASS
  Running: safe_string_bounds... PASS
  ...

=== Testing Input Validation ===
  Running: validate_date_format... PASS
  Running: validate_power_range... PASS
  ...

=========================================
Total:  15 | Passed: 15 | Failed: 0
All tests passed! ✓
=========================================
```

---

### Einzelne Tests ausführen

```bash
# Nur Config-Tests
./tests/test_config

# Nur String-Tests
./tests/test_safe_string

# Nur Validierungs-Tests
./tests/test_validation
```

---

### Test-Details

**Test-Module:**
- `test_config.c` – Config-Parsing, Integer/String, Validierung
- `test_safe_string.c` – String-Handling, Bounds-Checking
- `test_validation.c` – Input-Validierung (Datum, Leistung, etc.)

**Framework:** Custom C Test-Framework (`tests/test_framework.h`)

---

## Installation

### System-weite Installation

```bash
# Binary nach /usr/local/bin kopieren
sudo make install
```

**Installiert:**
- `/usr/local/bin/e3dcset` – Executable
- `/etc/e3dcset/e3dcset.tags` – Standard Tags-Datei (optional)

**Verwendung:**
```bash
# Von überall ausführbar
e3dcset -r EMS_BAT_SOC
```

---

### User-Installation

```bash
# In User-Verzeichnis installieren
mkdir -p ~/.local/bin
cp e3dcset ~/.local/bin/
cp e3dcset.tags ~/.local/share/

# PATH erweitern (in ~/.bashrc oder ~/.zshrc)
export PATH="$HOME/.local/bin:$PATH"
```

---

### Deinstallation

```bash
# System-weit
sudo rm /usr/local/bin/e3dcset
sudo rm -r /etc/e3dcset/

# User-Installation
rm ~/.local/bin/e3dcset
rm ~/.local/share/e3dcset.tags
```

---

## Plattform-spezifisch

### Linux (Debian/Ubuntu/Raspberry Pi)

**Standard-Build:**
```bash
sudo apt update
sudo apt install build-essential
make
```

**Raspberry Pi:** Funktioniert identisch zu Debian/Ubuntu.

---

### macOS

#### **Xcode Command Line Tools**

```bash
# Installieren
xcode-select --install

# Build
make
```

#### **macOS-spezifische Fixes (bereits implementiert)**

Der Code enthält bereits Fixes für macOS-Kompatibilität:

1. **Header-Includes:**
   ```cpp
   #ifdef __APPLE__
   #include <stdlib.h>  // statt malloc.h
   #endif
   ```

2. **Timestamp-Handling:**
   ```cpp
   #ifdef __linux__
   // Linux: time_t + long
   #elif __APPLE__
   // macOS: Nutzt timestamp.seconds/nanoseconds
   #endif
   ```

**Keine zusätzlichen Änderungen nötig!**

---

#### **Homebrew GCC (optional)**

Falls Apple Clang Probleme macht:
```bash
brew install gcc
make CXX=g++-13
```

---

### Arch Linux

```bash
sudo pacman -S base-devel
make
```

---

### Fedora/RHEL

```bash
sudo dnf install gcc-c++ make
make
```

---

## Fehlerbehebung

### Compiler nicht gefunden

**Fehler:**
```
make: g++: Command not found
```

**Lösung:**
```bash
# Debian/Ubuntu
sudo apt install build-essential

# macOS
xcode-select --install
```

---

### C++11 nicht unterstützt

**Fehler:**
```
error: 'auto' type specifier is a C++11 extension
```

**Lösung:**
- GCC Version ≥ 4.8 installieren:
  ```bash
  g++ --version  # Prüfen
  sudo apt install g++  # Aktualisieren
  ```

---

### malloc.h nicht gefunden (macOS)

**Fehler:**
```
fatal error: malloc.h: No such file or directory
```

**Status:** ✅ Bereits gefixt im Code!

**Falls weiterhin Fehler:**
```bash
# Code prüfen, sollte enthalten:
grep -r "__APPLE__" include/ src/
```

---

### Linking-Fehler

**Fehler:**
```
undefined reference to 'SocketConnect'
```

**Ursache:** Object-Dateien fehlen oder veraltet.

**Lösung:**
```bash
make clean
make
```

---

### Tests schlagen fehl

**Fehler:**
```
FAIL: config_parse_integer
```

**Debug:**
```bash
# Einzelnen Test ausführen mit Debug
./tests/test_config
```

**Häufige Ursachen:**
- Config-Datei fehlt oder falsch formatiert
- Berechtigungsprobleme

---

### Permission Denied beim Ausführen

**Fehler:**
```
bash: ./e3dcset: Permission denied
```

**Lösung:**
```bash
chmod +x e3dcset
```

---

## Makefile-Details

### Struktur

```makefile
CXX = g++
CXXFLAGS = -std=c++11 -Wall -I./include
LDFLAGS =
TARGET = e3dcset
BUILD_DIR = build

# Automatisches Dependency-Tracking
-include $(DEPS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) tests/test_*

test: $(TEST_TARGETS)
	@./tests/run_tests.sh
```

---

### Automatisches Dependency-Tracking

**Feature:** Makefile generiert automatisch `.d` Dateien für Header-Dependencies.

**Nutzen:**
- Änderungen an `.h` Dateien triggern automatisch Rebuild
- **Kein manuelles `make clean` mehr nötig** nach Header-Änderungen

**Beispiel:**
```bash
# Header ändern
nano include/config.h

# Nur betroffene Dateien werden neu kompiliert
make
# g++ -c src/config.cpp -o build/config.o
# g++ -c src/e3dcset.cpp -o build/e3dcset.o
# (Nur Dateien die config.h inkludieren!)
```

---

### Make-Targets

| Target | Beschreibung |
|--------|--------------|
| `make` | Standard-Build (kompiliert e3dcset) |
| `make all` | Synonym für `make` |
| `make clean` | Löscht alle Build-Artefakte |
| `make test` | Kompiliert und führt Tests aus |
| `make install` | Installiert System-weit (sudo) |

---

## Build-Variablen

### Compiler ändern

```bash
make CXX=clang++
```

---

### Flags überschreiben

```bash
# Custom CXXFLAGS
make CXXFLAGS="-std=c++14 -Wall -O3 -march=native"

# Custom LDFLAGS
make LDFLAGS="-static -lpthread"
```

---

### Build-Verzeichnis ändern

```bash
make BUILD_DIR=obj
```

---

## Cross-Compilation

### Raspberry Pi (auf x86_64 kompilieren)

```bash
# Cross-Compiler installieren
sudo apt install g++-arm-linux-gnueabihf

# Cross-Compile
make CXX=arm-linux-gnueabihf-g++
```

**Binary auf Pi kopieren:**
```bash
scp e3dcset pi@raspberrypi:/home/pi/
```

---

## Performance-Benchmarks

### Build-Zeit

**Typische Build-Zeiten:**
- **Raspberry Pi 4:** ~15 Sekunden
- **Intel i5 (4 Cores):** ~3 Sekunden
- **Apple M1:** ~1 Sekunde

**Clean Build:**
```bash
time make clean && time make
```

---

### Binary-Größe

**Typisch:**
- **Standard-Build:** ~200 KB
- **Debug-Build:** ~800 KB
- **Statisch gelinkt:** ~2 MB

```bash
ls -lh e3dcset
# -rwxr-xr-x 1 user user 198K Feb 10 12:00 e3dcset
```

**Optimierung:**
```bash
# Strip Debug-Symbole
strip e3dcset
ls -lh e3dcset
# -rwxr-xr-x 1 user user 120K Feb 10 12:00 e3dcset
```

---

## Continuous Integration

### GitHub Actions

**Workflow-Datei:** `.github/workflows/build.yml`

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Install dependencies
      run: sudo apt install build-essential
    - name: Build
      run: make
    - name: Test
      run: make test
```

**Badge:** `![Build](https://github.com/jarvis-schlappa/e3dcset/actions/workflows/build.yml/badge.svg)`

---

## Weitere Ressourcen

- **[USAGE.md](USAGE.md)** – CLI-Nutzung
- **[CONFIGURATION.md](CONFIGURATION.md)** – Konfiguration
- **[ARCHITECTURE.md](ARCHITECTURE.md)** – Code-Struktur

---

**Zurück zu:** [README](../README.md)
