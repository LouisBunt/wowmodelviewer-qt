# ModelViewer: Midnight — 3D-Druck. Ideen ohne Annahme über den Drucker.

Zweite Fassung. Die erste enthielt die Werte eines bestimmten Druckers als Prämisse — bei
einer Software, die veröffentlicht wird, ist das ein Konstruktionsfehler. Diese Fassung zieht
die Trennlinie zwischen **Funktion** und **Profildaten** und deckt alle Verfahren ab.

---

## Der Fehler, und die Gegenkorrektur

**Erste Fassung:** FDM als Prämisse. Folge: „vorgestützte Dateien" abgelehnt als „reines
Harz-Thema", die Düse als feste Größe in der Oberfläche, ein Bauraum hart im Code.

**Erste Korrektur, ebenfalls falsch:** „Harz ist die Norm in der Figurenszene." Durch Zahlen
**nicht gedeckt**. Belegt ist nur, dass der *bezahlte Tabletop-STL-Markt* harz-first
ausliefert — es existiert ein Werkzeug *Resin2FDM*, das harz-vorgestützte Miniaturen
FDM-tauglich macht, und keines in die Gegenrichtung. Das sagt etwas über den STL-Handel,
nichts über die Gerätebasis und schon gar nichts über WoW-Spieler.

**Was tatsächlich gilt:** Das Verfahren folgt der **Zielgröße**, nicht der Vorliebe.

| Figurhöhe | Auge der Figur | Verfahren |
|---|---|---|
| 28–32 mm Tabletop | ~0,5 mm | Harz; FDM ist Kompromiss |
| 75 mm Büste | ~1,2 mm | beides |
| 200–300 mm Statue | ~4 mm | FDM überlegen (Bauraum, Kosten, Zähigkeit) |

**Die Zielhöhe ist damit die wichtigste Eingabe des ganzen Features** — nicht der Druckertyp.

---

## Die dritte Gruppe, die beide Fassungen übersehen haben

Volltextsuche über die erste Fassung: **null Treffer** für Dienstleister, Shapeways, JLC,
SLS, MJF, Vollfarbe, 3MF. Das war die größte Lücke, und sie zerbricht die Binärannahme
„Harz oder FDM".

**Nicht jeder druckt selbst.** Wer bei einem Dienstleister bestellt, hat andere Anforderungen:

- **SLS/MJF** — was Dienstleister für haltbare Figuren fahren — braucht **keine Stützen,
  keine Ausrichtung, keine Kippung, keine Überhangprüfung**. Dafür Pulveraustrittslöcher und
  rund 0,7–1,0 mm Wandstärke. Ein drittes Verfahren, das den halben Harz-Teil gegenstandslos
  macht.
- **Harte Grenzen, die kein Slicer stellt:** Shapeways Vollfarbe nimmt DAE/WRL/X3D/OBJ mit
  Texturen als ZIP, **maximal 64 MB und 1 Million Polygone**. Ein Voxel-Remesh reißt das
  routinemäßig — **Dezimieren ist auf dem Dienstweg Pflicht, nicht Option.**
- **Rechtlich, weil veröffentlicht:** Zuhause drucken ist etwas anderes, als Blizzards
  Geometrie zu einem kommerziellen Dienst hochzuladen. **Kein „Direkt bestellen"-Knopf, keine
  Marktplatzanbindung.** Ein Hinweissatz genügt.

---

## Die Falle, die noch nicht zugeschnappt ist

**STL und OBJ tragen keine Einheit.** Slicer und Dienstleister nehmen Millimeter an.

Der FBX-Pfad dieses Projekts skaliert mit **91,44** — Yard nach **Zentimeter** — und schreibt
keinen Einheitenfaktor mit. Wer den Druckexport aus demselben Faktor ableitet, liefert eine
**zehnfach zu kleine Figur, ohne jede Fehlermeldung**.

Dieses Muster hat hier schon einmal zugeschlagen: Der OBJ-Exporter multipliziert mit 1,0 und
produziert Millimeter-Krümel. Beim zweiten Mal wäre es Fahrlässigkeit.

**Gegenmittel:** Der Druckexport rechnet **immer in Millimeter**, unabhängig vom FBX-Pfad,
und die Zahl steht in der Oberfläche, bevor exportiert wird. **3MF trägt die Einheit im
Format** und schließt den Fehler konstruktiv aus — ein Argument für 3MF, das in der ersten
Fassung fehlte.

---

## Der größte ungenutzte Vorteil: Farbe

Für einen WoW-Charakter **ist** Farbe der Punkt. Von Hand bemalen ist die größere Hürde als
drucken — und das Programm hat die Original-Texturen und das Material-Sidecar bereits.

- **Vollfarbdruck beim Dienstleister:** OBJ mit MTL und Texturen als ZIP, unter 1 Million
  Dreiecke. Kein Backen, kein Displacement, gebaut aus vorhandenen Bausteinen.
- **Bemalvorlage für Selbstbemaler:** Acht Ansichten, jede beleuchtet und flach unbeleuchtet,
  plus eine Farbkarte mit sprechenden Namen statt „Material_07". Der unbeleuchtete Render
  existiert als Verfahren bereits. Farbabgleich in CIELAB, nicht RGB — dort wird jedes Grau
  zu Braun.

**Herstellerfarbtabellen (Vallejo, Citadel) dürfen nicht mitgeliefert werden** — nicht frei
lizenziert. Hex und LAB ausgeben, den Abgleich dem Nutzer überlassen.

---

## Was universell ist

Unabhängig von Verfahren, Maschine und Material — und der eigentliche Wert des Werkzeugs,
weil **nur wir** diese Dinge wissen:

1. **Null-Dicke beheben** (Umhang, Rock, Tabard, Haarebenen). Der einzige echte Blocker, bei
   jedem Verfahren derselbe.
2. **Effektebenen erkennen und entfernen** — Glüheffekte, Billboards.
3. **Knochen und Geosets kennen:** Ein Schnitt auf Halshöhe verschwindet im Kragen, ein
   planarer Slicer-Schnitt landet quer durchs Gesicht.
4. **Texturen kennen** — für Bemalvorlage und Vollfarbe.
5. **Posen kennen:** Der Slicer darf das Objekt nur drehen. Die Pose ändern kann allein das
   Modellwerkzeug.
6. **Innengeometrie kennen** — siehe unten.

---

## Der WoW-spezifische Fund

**Hohlen setzt eine einzelne, wasserdichte Schale ohne Innengeometrie voraus.**

WoW-Charaktere haben systematisch Innengeometrie: der Körper unter der Rüstung, die Kopfhaut
unter dem Haar, Augäpfel und Zähne im Schädel, Beine unter dem Rock.

Beim **massiven** Druck ist das harmlos — der Slicer vereinigt überlappende Körper pro
Schicht. Beim **Hohlen** bricht es. Und Hohlen ist bei Harz ab Büstengröße Pflicht, weil sonst
der Saugnapfeffekt das Teil von den Stützen reißt oder eingeschlossenes Harz die Schale
aufsprengt.

**Das kann kein Slicer lösen, weil er nicht weiß, was innen ist — und es tritt nur bei
WoW-Modellen in dieser Härte auf.** Wenn das Werkzeug einen einzigartigen Beitrag leistet,
dann diesen.

Zur Einordnung: Hohlen ist **keine** Verfahrensschwelle, sondern eine **Größenschwelle**. Bei
28–32 mm ist die Empfehlung eindeutig massiv — hohlen macht so kleine Figuren nur zerbrechlich.

---

## Das Profil: sechs Felder, nicht sechzig

Ein Werkzeug, das erst nach dem Ausfüllen eines Maschinenformulars arbeitet, benutzt niemand.
Die Regel lautet: **Maße immer rechnen, Urteil erst in der Anzeige.**

| Feld | Wozu |
|---|---|
| Bauraum X/Y/Z | Passt es? Muss zerteilt werden? |
| Kleinstes Merkmal | Düsendurchmesser **oder** Pixelgröße — dieselbe Rolle |
| Mindestwandstärke | Solidify-Zielwert |
| `max_overhang_deg` | 90 heißt: Überhänge egal (SLS) |
| `default_tilt_deg` | 0 oder 30 — statt einer Verfahrensabfrage |
| `min_escape_hole_mm` | 0 = keine. Deckt Harz-Ablauf **und** SLS-Pulveraustritt |

Die letzten drei Zeilen sind der Trick: **Kein `if (verfahren == harz)` im Code.** Jede
Verfahrensbesonderheit wird zu einer Zahl, die auch ein unbekanntes viertes Verfahren
beschreiben kann.

**Ohne gewähltes Profil** rechnet das Programm trotzdem: Maße, Maßstab, Dreieckszahl. Nur die
Urteile („passt nicht auf den Bauraum") entfallen. Kein Null-Zweig, sondern ein echtes
Allgemeinprofil als Vorgabe.

**Profile beschaffen:** Nicht aus Slicer-Repositories übernehmen — PrusaSlicer-settings und
OrcaSlicer stehen unter AGPL-3.0, `PrusaSlicer-settings-prusa-fff` trägt **gar keine Lizenz**.
Cura ist LGPL-3.0. **Aus einem lokal installierten Slicer zu importieren ist keine Verbreitung
und damit gar keine Lizenzfrage** — das ist der saubere Weg.

---

## Die Prüfregel gegen den Fehler von letztem Mal

1. **Profil löschen.** Tut das Programm noch etwas Sinnvolles? Wenn es abstürzt oder schweigt,
   steckt eine Annahme im Code statt in den Daten.
2. **Werte absurd setzen** — Bauraum 10 mm, kleinstes Merkmal 5 mm. Kommen sinnvolle Warnungen
   heraus, oder rechnet irgendwo eine hart geschriebene Zahl weiter?

---

## Womit anfangen

Unverändert, und durch die Korrektur eher bestätigt: **der Blender-Operator „Für 3D-Druck
vorbereiten", abgenommen an einem einzelnen Teil.**

Er löst den einzigen Blocker, der bei **jedem** Verfahren auftritt, und braucht dafür kein
Profil und keine Verfahrensentscheidung. Alles andere — Prüfzeilen, Warnungen, Bemalvorlagen,
Sockel — ist Beratung rund um eine Kette, die es noch nicht gibt.

Danach in dieser Reihenfolge, weil jeder Schritt den nächsten trägt:

1. **Zielhöhe in Millimeter** samt Anzeige der echten Maße — schließt die Einheitenfalle
2. **Profil mit sechs Feldern** — macht aus Wissen eine Einstellung
3. **Segmentieren am Knochen** — der Punkt, an dem wir besser sind als jeder Slicer
4. **Farbe** — Bemalvorlage und Vollfarb-ZIP, der größte ungenutzte Vorteil

---

## Weiterhin abgelehnt

- **Eigene Mesh-Reparatur in C++** — Slicer verketten offene Konturen und vereinigen
  Überlappungen. Unser Vorsprung liegt allein im Skelettwissen.
- **Freier IK-Poseneditor** — ein Animationswerkzeug im Modellbetrachter. Die Posen*suche*
  liefert den Großteil davon in wenigen Tagen.
- **Galerie mit Server** — kein Feature, sondern ein Betrieb, und einer mit Blizzard-Assets.
- **Herstellerfarbtabellen mitliefern** — nicht frei lizenziert.
- **Checkbox „In T-Pose exportieren"** — das zugehörige Flag schaltet im OBJ-Weg zusätzlich
  die Anhänge ab und löscht still die Waffe.
- **Slicer-Profildaten mitliefern** — Lizenzlage. Importieren ja, verbreiten nein.

**Nicht mehr abgelehnt:** *Vorgestützte Dateien*. Die erste Absage stützte sich darauf, dass
der Entwickler FDM druckt — kein Argument für ein veröffentlichtes Werkzeug; für Harz ist es
die Auslieferungsnorm der Szene. Was wir beitragen können, ohne einen Stützengenerator zu
bauen: die **Stützensperrzone** über Gesicht und Brustfront. Nur wir wissen, wo das Gesicht
ist — und bei einer bemalten Figur ist die Stützennarbe im Gesicht der eine Fehler, den kein
Pinsel repariert.

---

## Was offen bleibt

- **Stufe 0 ist weiterhin nicht gelaufen.** Kein einziges Teil dieser Kette wurde je gedruckt.
  Jede Zahl oberhalb ist Recherche, keine Erfahrung.
- **Die Verteilung Harz gegen FDM unter WoW-Spielern ist unbekannt** und mit den vorliegenden
  Quellen nicht zu klären. Deshalb: verfahrensneutral bauen, statt zu raten.
- **Displacement aus Normalmaps** wurde als großer Hebel vorgeschlagen und in der Prüfung
  zurückgestuft: Eine Tangent-Space-Normalmap ist ein Gradient, keine Höhe; die Integration
  ist bei gespiegelten UVs schlecht gestellt. Kein Ein-Klick-Verfahren.
