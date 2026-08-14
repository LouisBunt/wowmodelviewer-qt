# 3D-Druck: vom Charakter zur Figur

Untersucht am 14.08.2026 gegen den Code dieses Projekts, den Quelltext von PrusaSlicer
und die Praxis der Leute, die WoW-Modelle heute schon drucken.

**Vorbehalt:** Die adversarische Gegenprüfung dieser vier Untersuchungen ist an einer
Sitzungsgrenze gescheitert. Die Befunde sind mit Datei- und Zeilenbelegen versehen, aber
nicht von einer zweiten Instanz angegriffen worden. Vor Stufe 2 gehört das nachgeholt.

---

## 1. Was wir bauen

Du stellst deinen Charakter in ModelViewer zusammen — oder lädst ein einzelnes Schwert,
einen Helm, eine Schulter. Ein Knopf sagt, wie hoch die Figur werden soll. Heraus kommt
eine Datei, die ein Slicer ohne Nachfrage annimmt und druckt.

---

## 2. Der ehrliche Kern

**Zwei Annahmen meiner ersten Analyse waren falsch, und beide in dieselbe Richtung:
zu pessimistisch.**

**„Löcher unter der Rüstung" gibt es nicht.** Ich hatte behauptet, ausgeblendete Geosets
rissen Löcher an Handgelenk, Hals und Knöchel. Das Basis-Körpergeoset mit der ID 0 wird
strukturell nie ausgeblendet — weder von `setGeosetGroupDisplay` (Bedingung `id > 0`,
`WoWModel.cpp:2873`) noch von `setCreatureGeosetData` (`WoWModel.cpp:2931`). Der Code
kommentiert das an `CharDetails.cpp:916-917` selbst als *„a clean bald rather than a hole
in the head"*. Unter der Rüstung liegt ein geschlossener Körper.

**Nicht-wasserdicht ist kein Blocker.** Moderne Slicer verketten offene Konturen aktiv:
PrusaSlicer, Orca und Bambu haben `chain_open_polylines_close_gaps` mit 2 mm Standardgrenze,
dazu einen eigenen Slicing-Modus „Close holes". Überlappende Körper werden pro Schicht
ohnehin vereinigt. Löcher und Durchdringungen erledigt die Werkzeugkette.

**Der eine echte Blocker ist Null-Dicke.** Umhang, Rock, Tabard und die Haar-Alphaebenen
sind Flächen ohne Volumen. Kein Volumen heißt keine Kontur — das repariert keine Bibliothek,
weil es kein Reparaturfall ist, sondern ein Fall von fehlendem Volumen. Dagegen hilft nur
Verdicken (Solidify) plus Voxel-Remesh.

**Daraus folgt der Zuschnitt:** Der ehrliche Weg führt über Blender, nicht über C++. Unser
FBX-Export liefert bereits verschweißte, indizierte Geometrie in Bindepose mit Skelett und
korrekter Zentimeter-Skalierung, und unser Blender-Addon erkennt Effektebenen schon
(`_is_effect_plane`). Eine eigene Reparatur-Pipeline in C++ wäre eine zweite, schlechtere
Kopie von etwas, das auf dem Rechner des Nutzers längst installiert ist.

---

## 3. Stufe 0 — heute, ohne eine Zeile Code

**Bevor irgendetwas gebaut wird: den Weg einmal bis zur G-Code-Datei fahren und den Fehler
messen statt schätzen.** Ein halber Tag.

1. Charakter in MV zusammenstellen, als FBX exportieren (Maßstab sitzt bereits,
   `FBXHeaders.h:56-61`)
2. In Blender mit unserem Addon importieren, „Effektebenen ausblenden" setzen
3. Armature-Modifier anwenden (Pose einfrieren)
4. Solidify auf Umhang/Rock/Tabard
5. Voxel-Remesh (Objekt vorher hochskalieren, sonst frisst der Remesh die Details)
6. Als STL exportieren, in den Slicer, slicen

**Das Ergebnis dieser halben Stunde entscheidet über alles Weitere.** Klappt es, ist Stufe 1
nur noch Bequemlichkeit. Klappt es nicht, wissen wir endlich *woran* — statt es zu vermuten.

---

## 4. Stufe 1 — ein Operator im vorhandenen Blender-Addon

**1–2 Tage. Der eigentliche Hebel.**

Neuer Operator „Für 3D-Druck vorbereiten" in
`upstream/blender_addon/io_import_wmv_fbx/__init__.py`, neben dem vorhandenen Import-Operator
und im selben Panel verlinkt. Er macht in einem Klick, was Stufe 0 von Hand macht:

| Schritt | Wie |
|---|---|
| Effektebenen löschen | `_is_effect_plane()` existiert bereits (`__init__.py:295`) — statt ausblenden jetzt entfernen |
| Pose einfrieren | Armature-Modifier anwenden |
| Dicke geben | Solidify je Objekt, wo das Material zweiseitig ist |
| Vereinigen | Voxel-Remesh mit Auto-Hochskalierung |
| Schreiben | STL-Export mit gesetzter Zielhöhe |

**Warum dort und nicht in C++:** Blender bringt OpenVDB für den Remesh mit, hat die
3D-Print-Toolbox, und der Nutzer hat es für unseren FBX-Weg ohnehin installiert.

---

## 5. Stufe 2 — STL-Exporter-Plugin, nur wenn Stufe 1 nicht reicht

**2–3 Tage.** Neues Plugin unter `upstream/Source/plugins/exporters/stl/` nach dem Muster von
`obj/`: vier Pflichtmethoden aus `ExporterPlugin.h:73-77`, eine Zeile in
`plugins/CMakeLists.txt`, eine Zeile in `installer/stage.ps1`. Danach erscheint das Format
**vollautomatisch** in Combobox, Menü, Format-Dialog, About-Box und Kommandozeile —
`ExportController.cpp:49-64` iteriert generisch, kein Format ist irgendwo namentlich genannt.

**Vorlage ist der FBX-Exporter, nicht OBJ.** Der OBJ-Exporter hat drei unabhängige Fehler:
Dreieckssuppe statt Indizes (`OBJExporter.cpp:228-257`), er liest einen bereits freigegebenen
VBO-Zeiger (`:232`), und er skaliert gar nicht (`:251`).

Die Bausteine, alle ohne eine einzige Änderung an `WoWModel.h` erreichbar:

- **Skinning selbst rechnen**, ~20 Zeilen nach `WoWModel.cpp:2296-2330`, ohne GL
- **Vertex-Dedup** wie `FBXHeaders.cpp:126-146` — das Pass-Gating muss in beiden Schleifen
  identisch sein, sonst läuft ein Index auf −1
- **Passes filtern** über ein reines Prädikat, ~12 Zeilen. Partikel und Ribbons brauchen
  keinen Filter: sie laufen über `drawParticles()` und tauchen in `passes` nie auf
- **Maßstab**: 1 Modelleinheit = 1 Yard = 914,4 mm. Nicht hartkodieren — Gnom und Tauren
  liegen um Faktor zwei auseinander. Bounding-Box der behaltenen Vertices messen und daraus
  auf die Zielhöhe rechnen
- **Windung**: Facettennormale immer aus dem Kreuzprodukt neu rechnen, nie aus dem Modell
  übernehmen. Bei gespiegelten Modellen (`mirrored_`) die Determinante prüfen

---

## 6. Stufe 3 — die Oberfläche

**3–4 Tage.** Ein eigener Abschnitt „FÜR DEN 3D-DRUCK" im Reiter *Export*, nach dem Muster
des „Als FBX exportieren"-Knopfs: eigener Knopf, der Format und Optionen **selbst** setzt.

**Kein fünfter Reiter** — die Reiterleiste hat vier Namen und `--tab 0..3` indiziert sie.
Ein Reiter „Druck" neben einem Reiter „Export" zwingt jeden zum Raten.

Dem Spieler ist **genau eine Zahl** zuzumuten:

```
Was wird gedruckt:  Ganze Figur                    [Nur ein Teil drucken …]
Höhe:               [ 100 ] mm      28 mm · 100 mm · eigene
                    Maßstab etwa 1:18 — aus 1,80 m werden 100 mm.

Höhe 100 mm · Breite 42 mm · Tiefe 38 mm · 84.000 Dreiecke
⚠ Umhang hat keine Dicke — wird in Blender verdickt.

           [ Für den Druck exportieren ]
```

Der wichtigste Fall ist **„Nur Teil"**: Ein einzelnes Schwert ist der wahrscheinlichere
Druck als eine ganze Figur. Drei Zustände existieren im Code bereits (Item allein geladen /
Fokus auf getragenem Teil / ganze Figur) — der Druckabschnitt baut dafür **kein eigenes
Bedienelement**, sondern benennt den Zustand in einem Satz und bietet den Ein-Klick-Weg in
den besseren an. Ein viertes Bedienelement wäre eine vierte Wahrheit.

---

## 7. Was wir nicht bauen

| Nicht | Warum |
|---|---|
| Eigene Reparatur-Pipeline in C++ | Löst das falsche Problem. Löcher und Durchdringungen erledigt der Slicer; Null-Dicke ist kein Reparaturfall |
| `manifold` (elalish) | **Verlangt** manifolde Eingabe. Kein Hole-Filling, keine Volumenerzeugung. Kategorisch falsches Werkzeug (und Apache-2.0, nicht MIT wie ich zuerst schrieb) |
| OpenVDB, CGAL, MeshFix | Abhängigkeitslawine für etwas, das Blender mitbringt und besser bedienbar macht |
| Wandstärken-Prüfer | Erst nach Solidify und Remesh sinnvoll — also in Blender, wo die 3D-Print-Toolbox es schon tut |
| Stützstrukturen, Sockel, Hohlkörper, Füllgrad | Aufgabe des Slicers, dort besser parametriert |
| Geosets zwangsweise einschalten | Macht den Stapel offener Schalen größer, nicht dichter |
| Haare aus der Alphatextur rekonstruieren | Bildverarbeitungsprojekt, kein Exporterfeature |
| `WoWModel.h` anfassen | Erzwingt Neubau **aller** Plugins — für null Gewinn, alles Nötige ist public |
| Checkbox „In T-Pose exportieren" | `bInitPoseOnlyExport` schaltet im OBJ-Exporter zusätzlich die Anhänge ab — die Checkbox würde still die Waffe löschen |

**Zwei Fallen aus dem Bestand, die man nicht mitkopieren darf:** Die Anhänge-Schleife in
`OBJExporter.cpp:162-200` prüft `showModel` nirgends — bei „Nur Teil" schriebe sie still die
ganze Figur, der schlimmste Fehlermodus, weil er wie Erfolg aussieht. Und der Druckknopf darf
**nicht** in `last_export.txt` schreiben: dieser Handschlag gehört dem Blender-Addon und
meint FBX.

---

## 8. Wie wir prüfen

Ohne Maus, mit vorhandenen Flags — sobald das Plugin existiert:

```
WoWModelViewer-Qt.exe --item-solo 12345 --export STL,C:\out\schwert.stl
WoWModelViewer-Qt.exe --mvlink MVM1:… --focus 3 --export STL,C:\out\schulter.stl
```

**Der eigentliche Beweis ist keine Prüfsumme, sondern ein gedrucktes Teil.** Ein Slicer, der
die Datei ohne Warnung annimmt und eine plausible Druckzeit meldet, ist die Abnahme.

---

## 9. Was offen bleibt

- **Stufe 0 ist noch nicht gelaufen.** Alles darüber ist Planung auf Grundlage von
  Codelektüre und Recherche, nicht auf Grundlage eines echten Drucks.
- **Die Gegenprüfung fehlt** (siehe Vorbehalt oben).
- **Resin statt FDM:** Bei 75–100 mm liegen Finger, Klingenschneiden und Hornspitzen unter
  der Breite einer 0,4-mm-Düse. Wer eine Figur will, druckt Resin — darauf sollte die
  Planung zielen.
- **Anhänge (Waffen, Schilde)** in Stufe 2 zunächst weglassen. Eine freischwebende
  Schwertspitze ist die häufigste Druckursache für Fehlschläge.
