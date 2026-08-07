## Item-Ansicht

Jede Zeile in der Ausrüstungsliste hat jetzt neben dem „×" ein Auge. Ein Klick blendet
den kompletten Charakter aus und zeigt nur dieses Teil — die Kamera rahmt es
automatisch, drehen und zoomen wie gewohnt. Nochmal klicken bringt den Charakter
zurück.

Das Auge erscheint nur bei Teilen, die eigene Geometrie haben. Brust, Beine, Hände und
Gürtel sind in WoW reine Texturen auf dem Körper und hätten allein nichts zu zeigen.

Beides funktioniert: aufgesetzte Teile (Waffen, Schultern — Schultern erscheinen als
Paar) und eingerechnete Teile (Helme, Gürtel).

Headless steuerbar über `--focus <slot>`.

## Export enthält nur noch, was sichtbar ist

Der FBX-Export ließ versteckte Flächen zwar weg, schrieb aber trotzdem jeden Vertex des
Modells in die Datei. Ein einzeln exportierter Helm schleppte damit die restlichen rund
317.000 Charakter-Vertices als lose Punkte mit — das blähte die Datei auf, verdarb
Bounding-Box und Objektursprung in Blender und ließ Blender eine Vertexzahl anzeigen,
die zu nichts auf dem Bildschirm passte.

| | vorher | jetzt |
|---|---|---|
| Helm einzeln | 319.688 Verts, 9,6 MB | 1.929 Verts, 1,19 MB |
| ganzer Charakter | 319.688 Verts | 20.065 Verts |

Der Inhalt bleibt gleich, das Skinning ebenso: null ungewichtete Vertices,
Gewichtssumme exakt 1,0.

## Kamera

Die automatische Rahmung hat versteckte Effektgeometrie mitgemessen — Glow-Flächen und
Ribbon-Emitter, die eine Waffe mit sich führt. Ein Zweihänder wurde dadurch als winziger
Fleck gerahmt. Behoben; zusätzlich wird jetzt die längste Achse eingepasst statt der
Raumdiagonale.

Bei langen, schräg liegenden Waffen bleibt etwas Luft im Bild — eine kugelbasierte
Rahmung kann das nicht vermeiden. Helme, Schultern und kompakte Teile sitzen satt.

## Getestet

- Wowhead-Decoder: 56/56 Fälle
- Blender 5.1: Helm rundum korrekt, Schultern mit beiden Seiten, voller Charakter
  unverändert
- ZIP ausgepackt und eigenständig gestartet: Import, Item-Ansicht und Export laufen

## Installation

Setup (`better-Model-Viewer-Setup-1.7.0-beta.exe`) installiert pro Benutzer, ohne
Adminrechte, und aktualisiert eine vorhandene Installation an Ort und Stelle. Wer nichts
installieren will, nimmt das ZIP und startet `WoWModelViewer-Qt.exe` direkt aus dem
entpackten Ordner.
