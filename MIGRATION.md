# WMV Frontend-Umbau: wxWidgets → Qt

Stand: 27.07.2026. Basis: `wowmodelviewer/wowmodelviewer` @ `adb2cb1` (develop).

## Ausgangslage (gemessen, nicht geschätzt)

| Modul | Zeilen / Dateien | wxWidgets? | Qt? |
|---|---|---|---|
| `Source/core` → `core.dll` | — | **nein**, 0 Includes | ja |
| `Source/games/wow` → `wow.dll` | — | **nein**, 0 Includes (2 Treffer sind Kommentare) | ja |
| `Source/plugins/*` | 4 Plugins | nein | ja (`QT_PLUGIN`) |
| `Source/wowmodelviewer` → `.exe` | **19.719 Zeilen, 81 Dateien** | ja, komplett | teilweise |

Das ist der entscheidende Befund: **Die gesamte Modell-, Render- und Datenschicht ist bereits
Qt-basiert und wx-frei.** Ein Qt-Frontend kann `core.dll` und `wow.dll` unverändert
weiterverwenden. Umzubauen ist ausschließlich die EXE.

Die größten Brocken darin:

| Datei | KB | Rolle |
|---|---:|---|
| `modelviewer.cpp` | 124 | Hauptfenster, Menüs, AUI-Docking, Import/Export-Orchestrierung |
| `maptile.cpp` | 69 | ADT/Kartenkacheln |
| `modelcanvas.cpp` | 55 | **OpenGL-Canvas** |
| `animcontrol.cpp` | 51 | Animationssteuerung |
| `charcontrol.cpp` | 38 | Charakterpanel |
| `app.cpp` | 34 | Anwendungsstart, Plugin-Laden |
| `filecontrol.cpp` | 25 | Dateibaum |

## Der kritische Pfad: der OpenGL-Canvas

`ModelCanvas` erbt von `wxGLCanvas`. Aber das eigentliche Zeichnen ist toolkit-unabhängig —
rohe GL-Aufrufe plus `root->draw()`, wobei `root` in `wow.dll` lebt:

```cpp
glClearColor(vecBGColor.x, vecBGColor.y, vecBGColor.z, 0.0f);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
root->draw();
SwapBuffers();
```

wx liefert nur drei Dinge: den GL-Kontext (`SetCurrent`), den Puffertausch (`SwapBuffers`) und
die Maus-/Tastaturereignisse. Genau die stellt `QOpenGLWidget` von sich aus bereit —
`makeCurrent()` und der Puffertausch passieren implizit rund um `paintGL()`.

**Bewertung:** Der Render-Code ist weitgehend übernehmbar. Zu ersetzen sind die Klassenbasis,
die Kontextverwaltung und die Event-Handler. Das ist der Grund, warum dieser Umbau überhaupt
realistisch ist.

## Initialisierung — ebenfalls schon Qt

```cpp
core::Game::instance().init(new wow::WoWFolder(dataFolder), new wow::WoWDatabase());
core::Game::instance().setConfigFolder(...);
```

Kein wx beteiligt. Ein Qt-`main()` kann das unverändert aufrufen.

## Phasenplan

Jede Phase endet mit etwas Lauffähigem. Kein "drei Monate nichts sehen".

### Phase 1 — Risiko ausräumen: Qt + GL + echtes Modell
Ein minimales Qt-Fenster, das gegen `core.dll`/`wow.dll` linkt, CASC initialisiert, ein M2 über
seine FileDataID lädt und in einem `QOpenGLWidget` zeichnet. Keine UI, kein Panel — nur der
Beweis, dass die Renderpipeline unter Qt lebt.

*Wenn diese Phase scheitert, ist der ganze Umbau tot.* Deshalb steht sie am Anfang.

### Phase 2 — Kamera und Interaktion
Maus-Orbit, Zoom, Pan, Hintergrundfarbe, Kamerapresets. Portierung der Event-Handler aus
`modelcanvas.cpp`.

### Phase 3 — Fenstergerüst
Titelleiste, Toolbar, Statusleiste, Dreispalten-Layout nach dem Entwurf. Der Prototyp unter
`C:\Users\braun\wmv-qt-proto` ist die Vorlage; sein Code ist direkt übernehmbar, weil dort
bereits mit denselben Design-Tokens gearbeitet wird.

### Phase 4 — Dateibaum und Modellauswahl
`filecontrol.cpp` → `QTreeView` mit eigenem Modell auf dem Listfile. Suchfeld, Kategorie-Chips.

### Phase 5 — Charakterpanel
`charcontrol.cpp` + `CharDetailsFrame.cpp` → Inspector aus dem Entwurf. Slider an
`CharDetails`, Ausrüstungsraster an die Item-Auflösung.

### Phase 6 — Animation
`animcontrol.cpp` → Timeline mit Scrubber.

### Phase 7 — Export und Plugins
Die Plugins sind bereits Qt; der Export-Dialog wird neu aufgebaut, die Plugin-Schnittstelle
bleibt.

### Phase 8 — Der Rest
Karten (`maptile.cpp`), Licht, Modellbank, Einstellungen, Lokalisierung.

## Was das kostet — ehrliche Einordnung

Phase 1–2 sind überschaubar. Phase 3–6 sind der Hauptteil. Phase 8 ist ein langer Schwanz aus
Kleinkram, der erfahrungsgemäß länger dauert als geplant.

19.700 Zeilen GUI werden nicht 1:1 übersetzt — ein guter Teil ist wx-Boilerplate, der in Qt
entfällt. Realistisch bleiben vielleicht 8.000–12.000 Zeilen neuer Qt-Code.

## Die unbequeme Wahrheit

Das Ergebnis ist eine **Abspaltung**. Upstream bleibt bei wxWidgets; jeder Fix von Rasmuslnd
müsste von Hand übernommen werden, solange er die GUI berührt. Fixes in `core`/`wow` — wie der
Mag'har-Fix — laufen dagegen weiter zusammen, weil diese Bibliotheken unangetastet bleiben.

Wer das trägt, muss klar sein, bevor Phase 3 beginnt. Phase 1 und 2 sind billig genug, um sie
als Experiment zu machen und danach zu entscheiden.

## Verzeichnisse

- `C:\Users\braun\wmv-src` — Upstream-Quellcode mit Mag'har-Fix (PR #17) und Dark Theme
- `C:\Users\braun\wmv-qt-proto` — statischer Design-Prototyp (Attrappe, keine Daten)
- `C:\Users\braun\wmv-qt` — dieser Umbau
