# Project

GridWorld — A* Pathfinding Simulation (C++ / SGG)

Interactive 2-player grid game with a live A* pathfinding agent.
Built as a C++17 project using the SGG graphics library and CMake.

## Τρέξιμο (SGG demo)

Οι εντολές παρακάτω τρέχουν από το root του project (τον φάκελο που έχει το `CMakeLists.txt`).

Θα χρειαστείς:
- CMake
- C++ compiler (π.χ. Visual Studio Build Tools σε Windows)
- Τα SGG sources (φάκελος `sgg-main/`) διαθέσιμα locally για build (δεν περιλαμβάνονται στην παράδοση)

Το SGG θέλει εξαρτήσεις (GLEW, glm, SDL2, Freetype, SDL2_mixer). Αν λείπουν, το CMake συνήθως σκάει με μήνυμα τύπου `Could NOT find GLEW`.
Σε Windows, ο πιο απλός/σίγουρος δρόμος είναι vcpkg (υπάρχει ήδη φάκελος `vcpkg/` στο project).

### 0) vcpkg (μία φορά)

```powershell
./vcpkg/bootstrap-vcpkg.bat
./vcpkg/vcpkg.exe install glew glm sdl2 sdl2-mixer freetype --triplet x64-windows
```

Αν δεν θες να τα γράφεις κάθε φορά (install + configure + build + run), υπάρχει και script:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run_sim_sgg.ps1
```

### 1) Configure (μία φορά)

```powershell
# Configure με vcpkg
cmake -S . -B build_sgg_vcpkg -DWITH_SGG=ON -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows

# Αν έχεις ήδη εγκατεστημένες τις εξαρτήσεις στο σύστημα (χωρίς vcpkg), μπορείς να δοκιμάσεις και:
# cmake -S . -B build_sgg -DWITH_SGG=ON
```

Αν έχεις ήδη έναν φάκελο `build_sgg` που είχε γίνει configure *χωρίς* vcpkg, μην προσπαθήσεις να του αλλάξεις toolchain μετά. Φτιάξε νέο build folder (π.χ. `build_sgg_vcpkg`) ή σβήσε τον παλιό.

### 2) Build

```powershell
cmake --build build_sgg_vcpkg --config Debug --target run_sim_sgg
```

### 3) Run

```powershell
.\build_sgg_vcpkg\Debug\run_sim_sgg.exe
```

### Αν "δεν τρέχει" ή δεν σε αφήνει να ξαναχτίσεις (LNK1168)

```powershell
taskkill /IM run_sim_sgg.exe /F
cmake --build build_sgg_vcpkg --config Debug --target run_sim_sgg
.\build_sgg_vcpkg\Debug\run_sim_sgg.exe
```

Αν σου βγάλει error τύπου `Error copying directory ... vcpkg/installed/$Triplet/...`, συνήθως σημαίνει ότι έχει μείνει “χαλασμένο” CMake cache. Τότε κάνε καθαρό rebuild:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run_sim_sgg.ps1 -Reconfigure
```



## Δομή φακέλων

- `src/` : κώδικας
- `include/` : headers
- `tests/` : unit tests (απλό harness)
- `docs/requirements.md` : σύντομη σύνοψη απαιτήσεων/κάλυψης

## Χαρακτηριστικά

| Feature | Περιγραφή |
|---|---|
| A* Pathfinding | Ο CPU agent χρησιμοποιεί A* με Manhattan heuristic |
| A* Visit Callback | Κάθε βήμα του A* εκπέμπει event (`open` / `closed` / `path`) μέσω callback |
| 2-Player input | P1: WASD, P2: Arrow Keys |
| CPU Agent | Autopilot με 3 επίπεδα δυσκολίας (Easy / Normal / Hard) |
| HUD Metrics | FPS, nodes expanded, path length, search time, match timer |
| Playback controls | Play/Pause, Step-by-step (N), Speed (-/+) |
| Map Selector | `M` εναλλάσσει μεταξύ example / large / huge maps live |
| In-app Map Editor | LMB = ζωγράφισε τοίχο, RMB = σβήσε τοίχο (στο setup mode) |
| Pause freezes timer | Ο χρόνος match σταματά όταν το game είναι paused |
| N-step mode | Παίκτες κινούνται, ο CPU agent παραλείπεται |
| Unit Tests | 9 Catch2 tests για A* correctness και Map::setCell |

## Πλήκτρα (in-game)

| Πλήκτρο | Ενέργεια |
|---|---|
| `SPACE` | Pause / Resume |
| `N` | Step 1 simulation tick (παίκτες κινούνται, CPU όχι) |
| `-` / `+` | Μείωση / Αύξηση ταχύτητας |
| `ENTER` | Έναρξη match |
| `R` | Restart στο setup |
| `M` | Επόμενος χάρτης (setup only) |
| `P` | Autopilot ON/OFF για επιλεγμένο agent |
| `C` | Εναλλαγή CPU difficulty |
| `Q` / `ESC` | Έξοδος |
| `LMB` (setup) | Ζωγράφισε τοίχο στο grid |
| `RMB` (setup) | Σβήσε τοίχο από το grid |

## Unit Tests (Catch2)

```powershell
# Μία φορά: configure με BUILD_TESTS=ON
cmake -S . -B build_sgg_vcpkg -DWITH_SGG=ON -DBUILD_TESTS=ON `
	-DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake `
	-DVCPKG_TARGET_TRIPLET=x64-windows

# Build tests
cmake --build build_sgg_vcpkg --config Debug --target pathfinding_test

# Run
.\build_sgg_vcpkg\Debug\pathfinding_test.exe
```

Αναμενόμενο output:
```
All tests passed (43 assertions in 9 test cases)
```

