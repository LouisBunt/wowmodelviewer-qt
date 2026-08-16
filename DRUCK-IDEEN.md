# ModelViewer: Midnight — 3D-Druck. Eine Liste.

49 Ideen aus vier Brillen, entdoppelt auf 28. Sortiert nach Nutzen pro Tag Arbeit.

---

## DIE EINE IDEE

**Der Blender-Operator „Für 3D-Druck vorbereiten", gefüttert aus einem `print`-Block im vorhandenen `.wmvmat.json`-Sidecar — abgenommen an EINEM Einzelteil bei 60–80 mm.**

Warum genau die: Es ist das einzige Vorhaben auf dieser Liste, das den einzigen bestätigten Blocker löst (Null-Dicke bei Umhang, Rock, Tabard, Haar-Alphaebenen). Alles andere — Prüfzeilen, Farbwarnungen, Bemalblätter, Sockel — ist Beratung rund um eine Kette, die es noch nicht gibt. Eine Warnung ohne Abhilfe ist eine Sackgasse: Sie sagt dem Nutzer nach zwei Sekunden, was er nach vierzehn Stunden ohnehin gesehen hätte, und lässt ihn genauso hilflos zurück.

Drei Belege, dass das die richtige Achse ist: Alle vier Brillen laufen unabhängig darauf zu. Der Fakt gilt als geprüft. Und FigurePrints, das dieses Produkt schon einmal verkauft hat, beschreibt exakt dieselbe Arbeit als seinen Kern — „identifying and thickening certain aspects of each model that were too thin to print".

Das Gerüst steht komplett: Panel, Registrierung, Handschlag, Ein-Klick-Import, `_is_effect_plane`, `_separate_by_material`, `_stamp_wmv_properties`, `BlenderAddonInstaller`. Der Operator ist ein zweiter Eintrag in derselben `draw()`-Spalte. Der Sidecar wird bereits geschrieben und bereits gelesen; ein additiver Schlüssel ist keine Formatänderung.

Und die Abnahme am Einzelteil ist Teil der Idee, nicht ein Anhängsel: **Stufe 0 ist nicht gelaufen.** Solange kein einziges Teil dieser Kette wirklich gedruckt wurde, ist jede Zahl weiter unten in dieser Liste eine Vermutung. Ein Helm dauert 40 Minuten und prüft dieselbe Werkzeugkette bis zum G-Code wie eine Figur, die 14 Stunden druckt und sechs Unbekannte gleichzeitig testet.

---

## Drei Entscheidungen, die die Liste vorwegnimmt

**1. Sidecar, nicht `last_export.txt`.** Brille 1 und Brille 2 widersprechen sich hier offen. Brille 2 will Zielhöhe und Düse als zusätzliche Zeilen in `last_export.txt` schreiben (2 Stunden), Brille 1 als Block im `.wmvmat.json` (1 Tag). Der Sidecar gewinnt: `last_export.txt` ist laut eigenem Kommentar „the whole contract" des FBX-Handschlags, trägt einen Pfad und sonst nichts, und wird atomar überschrieben. Druckzustand dort hineinzuschreiben schafft eine zweite Wahrheit über denselben Export und bricht die Regel aus DRUCK-KONZEPT.md an der schmalsten Stelle. Der Sidecar ist ohnehin additiv, versionstolerant (ältere Addon-Stände ignorieren unbekannte Schlüssel) und liegt garantiert neben genau diesem FBX. Die gesparten sechs Stunden sind den Vertragsbruch nicht wert.

**2. Alles Geometrische passiert in Blender.** Solidify, Remesh, Boolean, Bisect, Zapfen, Sockel, Fase. Im C++-Teil bleibt nur, was Blender prinzipiell nicht wissen kann: welches Geoset Stoff ist, welcher Knochen der Hals ist, welche Textur zu welchem Teil gehört, welche Pose es überhaupt gibt. Diese Trennlinie entscheidet über die halbe Liste.

**3. Das Druckerprofil ist eine Datei, kein Code.** Brille 1 will die MK3S-Zahlen „fest im Code" — das ist genau die Fassung, die bei der Veröffentlichung bricht. Brille 4s JSON unter `resources/` ist richtig, und der MK3S/Bear ist dann eines von mehreren mitgelieferten Profilen.

**Faktischer Fehler in Brille 4, bevor er ins Profil wandert:** Dort steht „Bauraum MK3S ist 250×210×210". Das ist der Serien-MK3S. Der Drucker des Nutzers hat einen Bear-Alurahmen und damit die genannten 320 mm Z. Ins Profil gehört **250 × 210 × 320**. Der bindende Wert ist ohnehin die Grundfläche, nicht die Höhe — genau der Spätschaden, den Brille 1 beschreibt.

---

## SOFORT

Erste Runde, zusammen etwa 12–14 Arbeitstage.

**S1 — `print`-Block im FBX-Sidecar** [1 Tag]
*(Brille 1 „Sidecar-Block", Brille 2 „Sidecar entscheidet, was verdickt wird")*
Zielhöhe, Düse, Druckerprofil, Maßstab, zurückgerechnete Solidify-Dicke t = D·U/H, Posenframe, Liste der Null-Dicke-Materialien, Segmentgrenze. Additiver Schlüssel neben `materials`/`bones`/`attachments`, nach dem Muster des `attachments`-Blocks. Voraussetzung für alles Weitere: Ohne ihn tippt der Nutzer dieselben zwei Zahlen zweimal, und die Solidify-Formel hängt an beiden.

**S2 — Blender-Operator „Für 3D-Druck vorbereiten"** [2–3 Tage]
*(Brille 1, Brille 2, Brille 4 — alle drei dieselbe Sache)*
Ein Knopf unter „Letzten WMV-Export importieren": Effektebenen löschen statt ausblenden, Pose einfrieren, **Maßstab anwenden**, Solidify nur auf die im Sidecar markierten Stoffobjekte, Voxel-Remesh nur auf diese, Boolean-Exact mit dem Rest, STL. Die Reihenfolge ist die Falle: Solidify vor dem Anwenden des Maßstabs erzeugt seitenweise unterschiedliche Wandstärken — unsichtbar am Bildschirm, sichtbar am eingerissenen Umhang. Von Anfang an so schreiben, dass er mit Parametern statt Panel-Feldern läuft; sonst ist P1 später nicht mehr erreichbar.

**S3 — Druckerprofil als JSON** [1 Tag]
*(Brille 1 + Brille 4; Brille 4s Fassung, Brille 1s Zahlen, korrigierter Bauraum)*
Bauraum, Düsenliste, Solidify-Faktor (3× Düsendurchmesser), Segmentmaximum, Spaltgrenze, Flag „offen/kein Input Shaping". Macht aus einer Regel, die der Nutzer wissen muss, eine Einstellung, die er auswählt. Mineways macht das seit Jahren genau so.

**S4 — Druck-Check im Export-Reiter** [Maßzeile allein 3 Stunden, mit Namensliste 2–3 Tage]
*(Brille 1 „zwei Zahlen, ein Urteil" + Brille 2 „Druckmaß-Zeile aus visibleBounds()")*
Zielhöhe und Düse rein, heraus kommen: Maßstab, echte Maße in mm, Dreieckszahl, Bauraum-Urteil — und eine **namentliche** Liste: „Umhang_01 hat keine Dicke, wird auf 1,2 mm verdickt". Anonyme Warnungen sind wertlos, weil der Nutzer in Blender vor 40 Objekten steht; die Namen stimmen automatisch, weil `_separate_by_material` die Objekte nach den Materialnamen benennt. `WoWModel::visibleBounds()` liefert die Messung bereits und ist public — heute wirft `frameVisible()` alles außer dem längsten Halbachsenmaß weg.

**S5 — „Testdruck: dieses Teil"** [0,5 Tag]
*(Brille 1)*
Nutzt den vorhandenen Teil-Fokus, setzt Zielhöhe auf 60–80 mm, exportiert nur das eine Stück. Kein neues Bedienelement — `setItemFocus`, `focusableSlots`, `--item-solo`, `--focus` und die Selbstausrichtung der Kamera existieren alle. Das ist der Knopf, der Stufe 0 hat.

**S6 — Graue Druckansicht** [1 Tag]
*(Brille 1 „graue Vorschau", Grundstufe von Brille 4 „farbige Vorwarnung" und Brille 3 „Röntgen")*
Texturen aus, mattes Hellgrau, Effektebenen weg, harte Seitenbeleuchtung. Die Textur lügt: Eine Rüstung, die durch Gold und Glüheffekte beeindruckt, ist als Silhouette oft eine formlose Masse. Ein Schalter, nicht drei — die farbige Einfärbung (L6) baut später auf demselben Renderpfad auf.

**S7 — Sockel, schlicht** [1–2 Tage]
*(Brille 1 + Brille 3 + Brille 4; ohne Gravur)*
Scheibe, Durchmesser aus der Figurbreite, Füße 1–2 mm eingesenkt, Boolean-Vereinigung. Zwei Schuhsohlen sind zu wenig Bettkontakt auf einem offenen Bettschubser, und ohne Sockel greift er die Figur beim Grundieren an den bemalten Stellen an. Die Fußposition steht als Knochen-Pivot im Sidecar, muss also nicht aus der Geometrie geraten werden. **Gravur später** — Mindeststrichstärke rund 0,8 mm, bei 0,4-mm-Düse nur auf großen Sockeln lesbar.

**S8 — Druckmappe: ein Ordner je Figur** [1,5 Tage, inkl. Vorschaubild je Datei]
*(Brille 1 „Druckmappe", Brille 2 „Vorschaubild neben jede Datei", Brille 4 „Rezept im Look")*
STL-Dateien, je ein PNG daneben, die `.chr`-Datei, und eine Karte im Klartext: Zielhöhe, Düse, Maßstab, Solidify-Dicke, Pose, Segmentliste, Datum. Bricht drei Wochen später ein Arm ab, braucht der Nachdruck **exakt** dieselben Werte — eine andere Zielhöhe bedeutet eine andere Solidify-Dicke und einen anderen Zapfendurchmesser, das neue Teil passt sonst nicht ans alte. Die billigste Idee der Liste, gemessen an verhindertem Frust. `saveLook()` macht Schreiben und Zuschneiden bereits wörtlich.

**S9 — Standpose statt Bindepose** [0,5 Tag]
*(Brille 2, kleine Fassung von Brille 1 und Brille 3)*
Clip wählen, pausieren, Frame in den Sidecar, Blender friert ihn ein. Der waagerecht abgespreizte Arm der Bindepose ist über seine volle Länge ein 90-Grad-Überhang — also Stützennarben genau dort, wo hinterher lackiert wird. Steuerung, Scrubber, `--anim` und `--clips` sind vorhanden; es ändert sich nur, welcher Frame es ist.

**S10 — Prüfteil-STL mitliefern** [1 Tag]
*(Brille 4)*
Ein Plättchen mit Wänden von 0,4 bis 1,6 mm und Spalten von 0,3 bis 1,2 mm, plus die Hand DIESES Charakters im gewählten Maßstab. Der gemessene Wert wandert ins lokale Profil, und ab dann rechnen alle Warnungen mit seinem Wert statt mit Literaturwerten. Das betrifft ausdrücklich die als gesetzt geltenden Zahlen: 360 mm für Fingerspalten bei 0,4 mm sind ein Startwert, kein Messwert an einem offenen Bear-MK3S. Eine halbe Stunde Druck beendet die Diskussion.

---

## LOHNT SICH

**L1 — Segmentieren am Knochen, mit Zapfen, in zwei Ordner** [4–6 Tage]
*(Brille 1 + Brille 3 + Brille 4 + Brille 4s „Grob/Fein")*
Schnitt an Hals, Handgelenk, Hüfte — die Ebene steht senkrecht auf der Knochenachse, die Knochen liegen im Sidecar. Zapfen mit 0,15–0,2 mm Spiel und angefaster Kante, einer je Fuge exzentrisch, damit es nur eine Steckrichtung gibt. Ausgabe in `0,4 mm/` und `0,2 mm/`, weil das zwei physische Druckaufträge mit Düsenwechsel sind. Der Gewinn gegenüber PrusaSlicers Cut-Tool ist ausschließlich, dass **wir die Gelenke kennen** und der Slicer nicht: Ein Schnitt auf Halshöhe verschwindet im Kragen, ein planarer Slicer-Schnitt landet quer durchs Gesicht. Den Cut-Dialog selbst nicht nachbauen.

**L2 — Waffe und Schild als eigenes Teil: flach, gefast, gesteckt** [2–3 Tage]
*(Brille 1 „Zapfen und Loch" + Brille 4 „flach legen und fasen")*
Eigene STL, auf die längste Achse gelegt, Schneide mit Mindestfase (eine Nullkante erzeugt im Slicer eine Kontur ohne Breite), 3-mm-Zapfen in ein 3,2-mm-Loch in der Faust. Drei Gründe auf einmal: die freischwebende Schwertspitze ist die häufigste Fehldruckursache, die Waffe braucht 0,2 mm während der Rumpf 0,4 läuft, und ein Schwert bemalt man getrennt in einer Minute statt in der Faust in zwanzig. Für ein Einzelteil ist Ausrichtung trivial und eindeutig — für eine ganze Figur wäre reine Überhangminimierung falsch (sie legt sie aufs Gesicht). Falle: nicht über den OBJ-Weg bauen, dessen Anhänge-Schleife prüft `showModel` nicht und schriebe bei „Nur Teil" still die ganze Figur.

**L3 — Prüfbericht aus Blender zurück** [2–3 Tage]
*(Brille 4)*
Nach dem Vorbereiten meldet das Addon in Klartext plus Miniaturbild: „Umhang von 0 auf 1,2 mm verdickt · 14 Haarebenen entfernt · dünnste verbleibende Stelle 0,9 mm (Schulterkante) · Volumen 61 cm³". Die 3D-Print-Toolbox rechnet Thickness, Overhang und Solid bereits; wir rufen sie nur auf und zählen die eigenen Änderungen mit. Eigene Datei, nicht `last_export.txt`. Das ist die billige Fassung des Menschen, den FigurePrints für die Endkontrolle beschäftigt hat.

**L4 — Bemalblatt, kleine Fassung** [3–4 Tage]
*(Brille 1 „vier Ansichten und eine Farbkarte" + reduzierte Fassung von Brille 3)*
Acht Ansichten, jede zweimal — beleuchtet und flach unbeleuchtet — plus eine benannte Farbkarte mit Hex und LAB: „Umhang außen", nicht „Material_07". Der Druck ist grau, der Charakter war genau deshalb interessant, weil er bunt war, und zwischen Export und Pinsel liegen Wochen. Der unbeleuchtete Render existiert als Verfahren bereits (`bakeCombinerTexture` rendert ausdrücklich ohne Licht), die Namen stehen im Sidecar, die Hautregionen in `CharTexture::LAYOUTS`. Farbabgleich in CIELAB per ΔE — in RGB wird jedes Grau zu Braun.

**L5 — Posenbewertung und Posensuche** [3–4 Tage]
*(Brille 3 + Brille 4; die Suche aus Brille 3, ohne den Editor)*
Drei Zahlen zur aktuellen Pose: Überhangfläche, Grundfläche, Schwerpunkt über der Auflage. Dann ein Knopf, der alle Frames aller Clips durchrechnet und die drei besten anbietet — rund 9.000 Auswertungen, Sekunden statt Minuten, ohne zu zeichnen. Das ist der Punkt, an dem das Programm etwas kann, was kein Slicer kann: Der Slicer darf das Objekt nur drehen, die Pose ändern kann nur das Modellwerkzeug.

**L6 — Farbige Vorwarnung im Viewport** [2–3 Tage, Aufsatz auf S6]
*(Brille 4, Grundstufe von Brille 3s „Röntgen")*
Rot, was keine Dicke hat; orange, was bei der eingestellten Höhe verschmelzen wird. Eine Zeile muss man glauben, eine rote Fläche kann man nachprüfen — und man sieht sofort, wie viel betroffen ist. Ausdrücklich **kein** echtes Dickenfeld: eingefärbt wird, was nur wir wissen (zweiseitige Passes, Effektebenen, Geoset-Zugehörigkeit).

**L7 — Fingerspalt und Klingenkante in Zahlen** [1–2 Tage]
*(Brille 1)*
„Fingerspalt 0,31 mm bei 200 mm Figurhöhe. Mit 0,4-Düse verschmelzen die Finger. Mit 0,2 getrennt ab 145 mm." Der einzige größenunabhängige Fehler der Kette, und der, den der Nutzer garantiert falsch einschätzt, weil „größer drucken = mehr Details" intuitiv, aber falsch ist. Reine CPU-Rechnung auf denselben Vertexdaten wie das Skinning. Erst nach S10 kalibrieren.

**L8 — Nach Material trennen: haut.stl, ruestung.stl, stoff.stl** [1 Woche]
*(Brille 3)*
Getrennte Dateien mit gemeinsamem Nullpunkt: Haut in hautfarbenem PLA drucken, nur die Rüstung bemalen. Die Trennung ist fertig (`_separate_by_material`), der ganze Aufwand ist die Zuordnung — aus `role`, Texturdateinamen und `CharTexture::LAYOUTS`. **Mit Korrekturliste bauen oder gar nicht**: eine falsch einsortierte Schulter sieht wie Erfolg aus. Funktioniert überhaupt nur, weil die Materialgrenze im M2 eine echte Geometriekante ist, keine gemalte Naht — die Teile stoßen spaltfrei aneinander.

**L9 — Stützensperre über Gesicht und Brustfront** [1–2 Tage]
*(Brille 4, nur die Sperr-Hälfte)*
Konvexe Hülle über die Kopf-Geosets als eigene STL, im Slicer in einem Griff als Support blocker geladen. Weil die Figur lackiert wird, ist eine Stützennarbe im Gesicht der eine Fehler, den kein Pinsel repariert — und nur wir wissen, wo das Gesicht ist. Die **Zwangskörper** aus derselben Idee streiche ich: Die automatischen Stützen setzen Überhänge ohnehin, ein Enforcer löst kein belegtes Problem.

**L10 — Material und Zeit grob vorrechnen** [2–3 Tage]
*(Brille 3, eigene Rechnung)*
Volumen per Divergenzsatz, daraus Gramm, Stunden, Euro, je Segment. ±30 % genügt vollkommen für die eine Entscheidung, die es ändert: dass die 300-mm-Fassung fünf Tage druckt und drei Spulen frisst. Nachtragfeld für gemessene Zeiten, damit der Faktor lernt. Den PrusaSlicer-CLI-Weg hier **nicht** einbauen — er gehört zu P1 und bringt dessen Abhängigkeit mit.

**L11 — Zwei kleine Aufräumer** [je 0,5 Tag, jederzeit vorziehbar]
*(Brille 2)*
Item-Browser-Filter „hat ein eigenes Modell" — Brust, Beine, Hände sind auf die Haut gemalt, und wer sie für einen Einzeldruck anklickt, bekommt eine Schaufensterpuppe; die SQL-Auflösung steht bereits in `standaloneModelFor`. Dazu `--print-parts`, das über `focusableSlots()` in **einem** Programmstart je Teil eine Datei schreibt — heute lassen sich `--focus` und `--export` nicht verschränken, und jeder Neustart mountet die CASC-Daten neu.

---

## SPÄTER

**P1 — Ein Knopf bis zur G-Code-Datei** [2–3 Wochen Bau, danach dauerhafte Last]
*(Brille 3)* Export → Blender `--background --python` → PrusaSlicer-CLI je Düse. Der eigentliche Preis sind nicht die Zeilen, sondern dass die Kette an drei fremden Programmen hängt und jede neue Version sie brechen kann — der Fehler landet dann als „das Programm ist kaputt" beim Autor. Nur mit sichtbarem Rückfallweg erträglich: bricht die Automatik, öffnet sie Blender mit dem geladenen Modell, statt zu scheitern. Voraussetzung ist, dass S2 von Anfang an UI-frei parametrierbar ist.

**P2 — Bemalvorlage, volle Fassung** [2–3 Wochen]
*(Brille 3)* Der Teil, den niemand sonst bauen kann: WoW-Texturen haben Schattierung, Schmutz und Kantenlichter **eingemalt**, die auf einer gedruckten Figur physisch nicht existieren. Trennt man je Fläche den dominanten Ton (die Basisfarbe, die man mischt) von der Abweichung (was Blizzards Maler an Tiefe hineingelegt hat), fällt die Wash- und Trockenbürstanleitung wörtlich aus der Textur. Das ist Blizzards eigene Malanleitung, rückwärts gelesen. Teuer sind Schattierungskarte und A4-taugliches PDF-Layout.

**P3 — Bauanleitung mit Explosionszeichnung und Reproduktionscode** [1–1,5 Wochen, setzt L1 voraus]
*(Brille 3)* Welcher Zapfen in welches Loch, Klebereihenfolge aus dem Segmentbaum, und ein Code, der genau diesen Export reproduziert. Die MVLink-Kodierung existiert samt Decoder; es kommen Felder dazu.

**P4 — Echte Wanddickenmessung im Viewport** [1–1,5 Wochen]
*(Brille 3)* BVH über die geskinnten Dreiecke plus Ray-Cast je Vertex. Widerspricht der Trennlinie (Blenders Toolbox misst das bereits) — deshalb erst, wenn L3 zeigt, dass der Bericht nach dem Vorbereiten zu spät kommt.

**P5 — Lokale Galerie: Ordner teilen, Import-Knopf** [1 Woche]
*(Brille 3, kleine Fassung)* Das Wertvollste an einem gelungenen Druck ist nicht die Datei, sondern die Einstellung, die dazu geführt hat. 90 % des Nutzens für einen Bekanntenkreis, ohne Server.

**P6 — Foto neben Render** [2–3 Wochen]
*(Brille 3)* Nur mit dem Fallback bauen, dass der Nutzer den Winkel grob hindreht und die Optimierung nur nachfeint — der Silhouettenabgleich verwechselt bei symmetrischen Posen gern 180 Grad.

**P7 — Sockelgravur mit Name und Wappen** [3–4 Tage, Aufsatz auf S7]
*(Brille 3 + Brille 4)* Der Name ist da (Armory `charName`, MVLink, `.chr`-Dateiname). Es scheitert nicht an der Technik, sondern an der Mindeststrichstärke — erst sinnvoll, wenn S10 gemessen hat, was bei 0,4 mm lesbar bleibt.

---

## NEIN

**N1 — Zielhöhe und Düse durch `last_export.txt`** *(Brille 2)*. Zweite Wahrheit über denselben Export, Bruch eines Vertrags, der ausdrücklich nur einen Pfad trägt. Gespart würden sechs Stunden. Siehe S1.

**N2 — Eine Checkbox „In T-Pose exportieren"** *(von Brille 1 und Brille 2 unabhängig als Falle erkannt)*. `bInitPoseOnlyExport` schaltet im OBJ-Weg zusätzlich die Anhänge ab und löscht damit still die Waffe. Ein Bedienelement, das etwas anderes tut als es sagt, ist schlimmer als keines.

**N3 — Zielhöhe durch `ExporterPlugin::setExportOptions`** *(Brille 2 nennt es selbst als Falle)*. Das ändert den Plugin-Header und erzwingt den Neubau aller vier Plugins. Vergisst man einen, meldet er still falsch. Der Druckweg darf die Plugin-ABI nicht anfassen — dafür gibt es den Sidecar.

**N4 — Freier IK-Poseneditor** *(Brille 3, große Fassung)*. 1–2 Monate Bau und danach dauerhafte Last: Jedes falsch gesetzte Gelenklimit erzeugt einen verdrehten Arm, jeder verdrehte Arm einen Fehlerbericht. Das ist ein Animationswerkzeug im Modellbetrachter. Die Posensuche (L5) liefert 80 % davon in vier Tagen und braucht den Editor nicht.

**N5 — Galerie mit Server** *(Brille 3, große Fassung)*. Kein Feature, sondern ein Betrieb: API, Bildspeicher, Moderation, Missbrauchsabwehr — und der harte Teil ist nicht technisch, sondern Blizzard-Assets in einem öffentlichen Bilderdienst, Nutzerfotos, DSGVO, Löschbegehren. Nicht anfangen, bevor jemand mit Server und Anwalt danebensteht. P5 kostet nichts davon.

**N6 — Vorgestützte STLs mitliefern.** Brille 4 hat das korrekt als Sackgasse identifiziert und ich übernehme die Absage: „pre-supported" ist ein reines Harzthema — Stützen sind dort nötig, weil jede Schicht von der FEP-Folie abgezogen wird. Für FDM mit PLA wären mitgelieferte Stützstäbe falsch; Hero Forge, der größte Anbieter der Szene, liefert bewusst ohne. Die FDM-korrekte Entsprechung ist L9.

**N7 — Eigene Mesh-Reparatur in C++** (Löcher schließen, Durchdringungen auflösen, planares Zerteilen mit Dübeln nachbauen). Gilt laut geprüften Fakten als Nicht-Blocker: Slicer verketten offene Konturen und vereinigen überlappende Körper pro Schicht, und PrusaSlicer hat das Cut-Tool mit Dübeln seit Jahren. Alles, was wir hier nachbauen, ist teurer und schlechter. Unser Vorsprung liegt allein darin, dass wir das Skelett kennen (L1).

**N8 — Ein „gut druckbar"-Abzeichen im NPC-Browser** *(Brille 2)*. Die Beobachtung dahinter ist wertvoll und gehört in die Dokumentation: Kreaturenmodelle haben meist keinen Umhang, kein Tabard, keinen Rock und keine Haar-Alphaebenen, also entfällt bei ihnen der einzige echte Blocker — 23.031 leichte Druckobjekte stehen bereits durchsuchbar da. Aber als Abzeichen wäre es ein zweiter Klassifikator neben S4, der dasselbe misst und irgendwann etwas anderes sagt. Ein Druckknopf direkt aus der Liste (drei Stunden) ja, eine zweite Prüfung nein.

**N9 — Herstellerfarben (Vallejo, Citadel, AK) als Zuordnungstabelle ausliefern** *(Teil von Brille 3)*. Die Tabellen sind nicht frei lizenziert. Hex und LAB liefern, den Abgleich dem Nutzer oder einer offenen Quelle überlassen. Bei einem Programm, das veröffentlicht wird, ist das kein Detail.

**N10 — Ein hart einkodiertes MK3S-Profil** *(Brille 1)*. Funktioniert für genau einen Menschen und ist bei der Veröffentlichung die erste Sache, die jemand fragt. S3 kostet einen halben Tag mehr.

---

## Was nur für diesen Nutzer taugt

**Nur für ihn — als Daten, nicht als Code:**
- Das MK3S/Bear-Profil selbst (250 × 210 × 320, offen, kein Input Shaping, Segmentgrenze 120–150 mm). Die Profil-**Mechanik** ist für alle, der Inhalt ist eine Zeile in einer JSON.
- Der gemessene Prüfteil-Wert aus S10. Gehört in sein lokales Profil unter `userSettings`, niemals in den Auslieferungsstand — sonst erbt jeder Nutzer die Messwerte eines fremden Druckers.
- Die Grob/Fein-Zweiteilung aus L1 spiegelt seine Arbeitsweise (ein Drucker, Düsenwechsel von Hand). Wer eine MMU oder zwei Drucker hat, will eine andere Aufteilung. Die Einstufung je Segment gehört deshalb ins Profil, nicht in den Operator.
- P6 (Foto) ist praktisch ein Ein-Personen-Feature.

**Für alle, ohne Abstriche:** S1, S2, S4, S5, S6, S7, S8, S9 sowie L1–L5, L7, L8, P3. Das ist der Kern, und er ist unabhängig von Drucker, Material und Arbeitsweise — die eine Sache, die jeder braucht, ist die Behebung der Null-Dicke, und sie ist bei jedem dieselbe.

**Zielgruppenabhängig, aber unschädlich:** L4/P2 (Bemalvorlage) nützt nur Lackierern — das ist eine große Gruppe und kostet niemanden etwas, der sie nicht öffnet. L8 (Materialtrennung) zahlt sich erst bei mehreren Filamentfarben richtig aus.

---

## Zum Aufwand, ehrlich

Keine Idee dieser Liste erzwingt eine Änderung an `WoWModel.h`. `visibleBounds()` ist public und im Header deklariert, die Überhang- und Spaltrechnungen laufen im Frontend über dieselben Vertexdaten, die das Skinning ohnehin erzeugt, und der FBX-Exporter schreibt bereits ausschließlich Vertices, die ein sichtbarer Pass indiziert — der „nur Sichtbares"-Filter, den man für den Druck bauen würde, existiert also schon (eine Stunde Verifikation, null Code).

Das ist kein Zufall, sondern die Bedingung: In dem Moment, in dem jemand eine Hilfsmethode in `WoWModel.h` bequem findet, kostet es den Neubau aller Plugins — und wenn einer vergessen wird, meldet er still falsch. Wer eine Idee dieser Liste umsetzt und dabei den Header anfassen will, hat den falschen Weg gewählt; die Antwort liegt fast immer im Sidecar oder in Blender.