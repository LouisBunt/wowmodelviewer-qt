# Das Logo austauschen

Das MV-Symbol und das Ladebild sind Platzhalter. Ein eigenes Logo ersetzt sie, indem du die
Dateien unten **mit demselben Namen und in derselben Größe** überschreibst. Mehr ist nicht
nötig: Beim nächsten Bau (oder Push auf GitHub) steckt das neue Logo im Programm und im Setup.

| Datei | Größe | Wo man sie sieht |
|---|---|---|
| `appicon.ico` | mehrere Größen in einer Datei: 16, 20, 24, 32, 40, 48, 64, 128 und 256 px | Programmdatei im Explorer, Taskleiste, Verknüpfungen, das Setup selbst |
| `appicon.png` | 512 × 512 px | Fenster „Über MViewer“ und „Kontakt“, großes Fenstersymbol |
| `appicon-16.png`, `appicon-24.png`, `appicon-32.png`, `appicon-48.png` | 16, 24, 32 und 48 px | Titelleiste und kleines Fenstersymbol |
| `splash.png` | 480 × 360 px | Ladebild beim Start |

Worauf es ankommt:

- **Kleine Größen eigens zeichnen.** Ein großes Logo, einfach auf 16 px verkleinert, wird
  unleserlich. Deshalb gibt es die Dateien `appicon-16.png` bis `appicon-48.png`: dort lieber
  dickere Linien und weniger Details.
- **Durchsichtiger Hintergrund** bei den Symbolen (PNG mit Transparenz), damit sie auf hellen
  und dunklen Flächen gut aussehen.
- **Ladebild: Rand in einer einzigen Farbe.** Unter dem Bild zeigt das Programm den
  Ladefortschritt. Ist das Fenster breiter als das Bild, füllt es den Rest mit der Farbe der
  linken oberen Ecke. Ein Verlauf am Rand würde dort eine Kante zeigen. Die Breite von 480 px
  passt zum Text darunter.
- **`appicon.ico`** lässt sich zum Beispiel mit GIMP (Exportieren als `.ico`, alle Größen
  ankreuzen) oder einem Online-Umwandler aus den PNG-Dateien machen.

Der Schriftzug „MVIEWER“ in der Titelleiste und im Setup ist Text, kein Bild. Er steht in
`MainWindow.cpp` (`brandLabel_`) und in `installer/setupui/main.cpp` (`Sidebar`).

Die Platzhalter selbst zeichnet `tools/make-logo.py` (braucht Python und Pillow). Für ein
eigenes Logo wird das Skript nicht gebraucht.
