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

## 1a. Zielgröße: große Figuren

**Entscheidung des Projektinhabers (14.08.): Ziel sind große Figuren, nicht Tabletop.**

Recherchiert und adversarisch gegengeprüft. Die Gegenprüfung hat drei Sachfehler der
Recherche korrigiert — und einen Denkfehler in meiner eigenen Argumentation.

### Der Denkfehler: nicht die Breite ist das Problem, sondern der Spalt

Ich hatte gesagt, größere Figuren lösten das Detailproblem, weil ein Finger dann breiter ist
als die Düse. Das misst die falsche Größe.

**Konvexe Details** — Nase, Fingerbreite, Muskelrelief — überleben ab etwa **120–150 mm**.
Sie liegen als Relief auf einer massiven Fläche, der Perimeter fährt ihre Kontur nach, und
die Positioniergenauigkeit liegt bei rund 10 µm, nicht bei der Düsenbreite.

**Konkave Trennungen** — der Spalt *zwischen* zwei Fingern, Kettenglieder, Haarsträhnen,
Schnallenspalte — überleben bei 0,4-mm-Düse **auf keiner realistischen Figurgröße**. Zwei
Finger mit 2 mm realem Abstand verschmelzen, solange der Spalt schmaler als eine Düsenbreite
ist: bis etwa **360 mm** Figurhöhe, sauber getrennt erst ab rund 720 mm.

> Das ist das „Finger verschwinden" aus meiner ersten Analyse — und es verschwindet bei
> 200 mm **nicht**. Es ist nur kein Auflösungs-, sondern ein Geometrieproblem.

Für die Klingenschneide gilt dasselbe aus demselben Grund: scharf zulaufend bräuchte sie
1,5 m Figurhöhe. Sie muss beim Export eine **Mindestfase** bekommen, sonst erzeugt der
Slicer eine Nullkante.

### Die Düse ist eine Variable, kein Naturgesetz

Alle Schwellen halbieren sich mit einer **0,2-mm-Düse** — der billigste Hebel überhaupt, und
A1, P1S, X1C und MK4S nehmen sie alle. Dazu kommt Arachne (Standard in PrusaSlicer 2.5+,
Orca, Bambu Studio), das Einzelwände mit variabler Breite unter der nominellen Bahnbreite
druckt; der harte Cutoff liegt real eher bei 0,25–0,3 mm.

**Folge fürs Werkzeug:** Die Solidify-Vorgabe darf nicht fest 1,2 mm sein, sondern
**3 × Düsendurchmesser** (0,6 / 1,2 / 1,8 / 2,4 mm). Die Düse gehört damit in die
Oberfläche — sie ist die zweite und letzte Zahl, die der Nutzer eintippen darf.

### Die Solidify-Formel — bestätigt, mit einer Falle

Figur `U` Modelleinheiten hoch, Zielhöhe `H` mm, gewünschte Enddicke `D` mm:

```
Maßstab   S = H / U          [mm pro Modelleinheit]
Solidify  t = D / S = D · U / H
```

Mensch (U = 2), H = 250 mm, D = 1,2 mm → **t = 0,0096** Modelleinheiten.

**Die Falle** steht im Blender-Handbuch: *„The modifier thickness is calculated using local
vertex coordinates. If the object has a non-uniform scale, the thickness will vary on
different sides of the object."* Der Operator muss den Maßstab also **anwenden**, bevor er
Solidify setzt. Für die Voxelgröße im Remesh gilt dieselbe Formel und dieselbe Falle.

### Voxel-Remesh: die Sorge war unbegründet, die Empfehlung bleibt

„Mehrere hundert Millionen Voxel, das schafft Blender nicht" war um ein bis zwei
Größenordnungen falsch — OpenVDB arbeitet dünnbesetzt, eine 300-mm-Figur ergibt rund
2 Millionen aktive Voxel. Normale Sculpting-Praxis.

**Der Remesh gehört trotzdem nicht über alles**, aber aus einem anderen Grund: Er rundet
*jede* scharfe Kante um etwa eine Voxelbreite ab, auch alle Rüstungskanten. Richtig ist
deshalb: **nur die verdickten Stoffteile remeshen**, den geschlossenen Rest per
Boolean-Vereinigung (Exact-Solver) dazunehmen. Das erhält die Kanten.

### Bauraum: die 256er-Wand

| Höhe | Lage |
|---|---|
| bis 150 mm | überall, auch A1 mini (180 mm) |
| bis ~230 mm | einteilig auf fast allem; MK4S hat 220 mm |
| ab ~250 mm | auf dem verbreitetsten Bauraum (Bambu A1/P1S/X1C, 256 mm) **zerteilen** |
| ab 330 mm | auch auf Großformatgeräten zerteilen, außer Neptune 4 Plus/Max |

Geprüft: Bambu A1/P1S 256³, Prusa MK4S 250×210×220, CORE One L 300×300×330.

### Die Pose: Bindepose ist zum Drucken die schlechtere

Ein waagerecht ausgestreckter Arm ist über seine volle Länge ein 90-Grad-Überhang. Eine
Standpose mit angelegten Armen druckt sich deutlich besser als die T-Pose — ein weiteres
Argument dagegen, den Druckexport an `bInitPoseOnlyExport` zu hängen.

### Was das für den Zuschnitt heißt

Der Kern des Plans bleibt: über Blender, nicht über C++. Dazu kommen drei Dinge, alle klein:
skalierende Solidify-Dicke aus Zielhöhe und Düse, ein Bauraum-Hinweis, und **die Erkenntnis,
dass Fingerspalten und Klingenschneiden kein Größenproblem sind** — sie brauchen entweder
eine geometrische Öffnung, eine feinere Düse, oder werden bewusst als Fäustling akzeptiert.

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
| Dicke geben | Solidify je Objekt, wo das Material zweiseitig ist — **Dicke aus der Zielhöhe zurückgerechnet**, damit der Umhang bei jeder Figurgröße dieselbe gedruckte Wandstärke hat |
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
Höhe:               [ 200 ] mm      100 · 200 · 300 mm · eigene
Düse:               [ 0,4 ] mm
                    Maßstab etwa 1:9 — aus 1,80 m werden 200 mm.

Höhe 200 mm · Breite 84 mm · Tiefe 76 mm · 84.000 Dreiecke
⚠ Umhang hat keine Dicke — wird in Blender auf 1,2 mm verdickt.
⚠ Fingerspalten unter 0,4 mm — die Hand wird als Fäustling gedruckt.

           [ Für den Druck exportieren ]
```

Die Vorgabe ist **200 mm**, nicht 100: Das ist die Größe, bei der ein gewöhnlicher
FDM-Drucker die Details noch trifft und die Figur trotzdem auf das Bett passt. Überschreitet
die eingetragene Höhe den Bauraum verbreiteter Drucker, kommt eine Zeile dazu:

```
⚠ 300 mm passt auf die meisten Drucker nicht — in Blender oder im Slicer zerteilen.
```

Kein Bedienelement zum Zerteilen. Nur der Hinweis, wo es hingehört.

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
- **Zerteilen großer Figuren** bleibt bewusst außerhalb: PrusaSlicer (Taste `C`, planar oder
  Schwalbenschwanz, mit Zapfen/Dübeln) und Bambu Studio können es, wir hätten nur eine
  schlechtere Kopie. Offen ist, ob ein Warnhinweis reicht oder der Nutzer Führung braucht.
- **Fingerspalten und Klingenschneiden** sind ungelöst und größenunabhängig. Drei Wege, alle
  ungeprüft: geometrisch aufweiten im Export, feinere Düse empfehlen, oder als Fäustling
  akzeptieren und das ehrlich anzeigen. Was davon trägt, weiß erst Stufe 0.
- **Die Materialschätzung der Recherche war frei gefudged** (±50 % Streuung, mit drei
  signifikanten Stellen präsentiert) und ist hier bewusst nicht übernommen. Nebenbefund, der
  trägt: Bei 200–300 mm ist die 1,2-mm-Schale schon rund 44 % des Vollvolumens —
  **Hohldruck spart in dieser Größe kaum etwas.**
- **Anhänge (Waffen, Schilde)** in Stufe 2 zunächst weglassen. Eine freischwebende
  Schwertspitze ist die häufigste Druckursache für Fehlschläge.
