# WORMS 3D — il verme che cresce

Serpente tridimensionale per Linux in un'arena cartesiana 32 x 18 x 32. Si vola
liberamente nello spazio (imbardata + beccheggio), si mangia di tutto e il corpo
cresce davvero: ogni frutto ha un effetto diverso. Rocce da evitare, bombe che le
fanno esplodere, tre vite e un punteggio record persistente.

C++20 + Qt6 + OpenGL 3.3 core. Nessuna texture, nessun modello, nessun file
audio: mesh, cielo, griglia e suoni sono generati a runtime.

## Compilare e giocare

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/worms3d
```

Opzioni:

```sh
./build/worms3d --fullscreen
./build/worms3d --size 1920x1080
./build/worms3d --autopilot
./build/worms3d --capture /tmp/shots --scenario auto --seconds 20 --shots 6
./build/worms3d --capture /tmp/shots --scenario showcase --shots 1
./build/worms3d --capture /tmp/shots --scenario loop --seconds 2 --shots 4
```

Requisiti: Linux con OpenGL 3.3, Qt 6.5 o successivo (Core, Gui, Widgets,
OpenGL, OpenGLWidgets, Multimedia). Su Ubuntu: `qt6-base-dev
libqt6opengl6-dev qt6-multimedia-dev cmake g++`.

## Comandi

| Tasto | Azione |
| --- | --- |
| A / D (o frecce) | imbardata a sinistra / destra |
| S / W (o frecce) | muso in alto / in basso |
| SHIFT o SPAZIO | scatto (consuma la barra SCATTO) |
| P | pausa |
| R | ricomincia |
| H | mostra/nascondi aiuto |
| M | muto |
| G | griglia cartesiana on/off |
| O | autopilota (dimostrazione) |
| F | schermo intero |
| Ctrl+Q | esci |

La telecamera sta **dietro e sopra** il verme, come in uno sparatutto in terza
persona: si vede il personaggio di spalle e il corpo che si allunga dietro di
lui. Si guida con la tastiera; un mirino fisso al centro indica la direzione
del muso. Se la vista si incastra dietro una roccia, la camera si avvicina da
sola invece di finirci dentro.

## Regole

- Il verme avanza sempre. Si sterza, non si frena.
- Il beccheggio non ha limiti: si possono fare giri completi e volare
  capovolti. Il corpo però resta la scia del muso, quindi un giro chiuso con
  un verme lungo ti fa mordere la coda: per girare "all'infinito" usa una
  spirale (beccheggio + un po' di imbardata).
- Mangiare fa crescere; il corpo è la scia del muso e **mordersi è letale**.
- I bordi dell'arena rimbalzano (con piccolo malus), le rocce no: schiantarsi
  contro una roccia o il proprio corpo costa una vita.
- Ogni 8 frutti "buoni" sale il livello: il verme accelera e compaiono più
  frutti e più rocce.
- Le bombe distruggono cibo e rocce nel raggio di 5,5 unità (e fanno punti).
- Il record è salvato in `~/.config/pseudocodice/WORMS 3D.conf`.

## Cibo ed effetti

| Frutto | Forma | Punti | Crescita | Effetto |
| --- | --- | --- | --- | --- |
| Mela | sfera rossa | +10 | +1 | — |
| Mela d'oro | sfera dorata | +75 | +2 | scade dopo 16 s |
| Peperoncino | ellissoide arancio | +20 | +1 | TURBO 6 s (x1,75) |
| Mirtillo | sfera blu | +20 | +1 | TEMPO LENTO 5 s (sterzo più ampio) |
| Limone | ellissoide giallo | +25 | +1 | PUNTI x2 per 10 s |
| Fungo velenoso | stella viola | −25 | −3 | AVVELENATO 4 s, tinta viola |
| Stella | stella dorata | +50 | +1 | INVINCIBILE 8 s (attraversa corpo e rocce) |
| Calamita | ciambella rossa | +40 | +1 | CALAMITA 8 s (attira il cibo) |
| Frutto bomba | sfera scura con miccia | +40 | −1 | esplosione: distrugge cibo e rocce |
| Gemma vita | ottaedro rosa | +100 | +1 | +1 vita (max 5) |
| Cristallo | ottaedro ciano | +40 | +1 | SCUDO: assorbe un colpo mortale |

I frutti con durata lampeggiano negli ultimi 4 secondi. I potenziamenti si
rinnovano (non si sommano) raccogliendo lo stesso frutto.

## Come è fatto

```
src/core/    simulazione pura, zero Qt/GL: World, Snake, Effects, cibo, RNG,
             autopilota deterministico
src/render/  mesh procedurali, wrapper GL (QOpenGLExtraFunctions), renderer
             istanziato: cielo, griglia, assi, mesh, aloni additivi, polvere
src/app/     QOpenGLWidget (input, camera, HUD QPainter, modalità capture),
             HUD, toast, minimappa
src/audio/   sintetizzatore in pull mode su QAudioSink (48 kHz, int16)
shaders/     GLSL 330 core dentro shaders.qrc
tests/       selftest headless della sola simulazione
```

Scelte che contano:

- **Mondo puro e deterministico.** `World::step(dt, input)` non tocca Qt: gli
  stessi seed producono gli stessi frame, ed è ciò che rende verificabili le
  catture (`--capture`) e i test.
- **Una tabella per il cibo.** `src/core/food.cpp` descrive forma, colore,
  alone, punteggio, crescita, effetto, durata, vita e probabilità di spawn.
  Renderer e HUD leggono la tabella: aggiungere un frutto è una riga.
- **Istanze, non oggetti.** Sfere, cubi, stelle, ottaedri e ciambelle sono
  disegnati con `glDrawElementsInstanced`: un draw call per forma, anche con
  centinaia di segmenti e particelle.
- **HUD in QPainter** sopra il frame GL: testo nitido senza texture né font
  bitmap.
- **Audio senza asset.** I cue sono voci sinusoidali/rumore con inviluppo
  esponenziale, mixate in un `QIODevice` che `QAudioSink` legge in pull mode.
  Se non c'è un dispositivo audio il gioco resta muto, senza errori.

## Verifica

```sh
./build/worms_selftest     # oppure: ctest --test-dir build
```

Il selftest copre: integrità del catalogo, crescita con la mela, accorciamento
da veleno, turbo più veloce, rimbalzo sui bordi, scudo che assorbe un colpo e
morte successiva, esplosione della bomba, determinismo su 600 passi, autopilota
che mangia e resta vivo, equidistanza dei segmenti del corpo, convenzione degli
assi di sterzo, progressione di livello.

Per guardare i frame senza aprire finestre (utile in CI o via SSH):

```sh
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 \
  xvfb-run -a -s "-screen 0 1600x900x24" \
  ./build/worms3d --capture /tmp/shots --scenario auto --seconds 20 --shots 6
```

`--scenario showcase` mette in fila un esemplare di ogni frutto con il verme in
posa: serve a giudicare forme e colori di tutte le varianti in un colpo solo.
`--scenario loop` tiene il muso in alto per due secondi e cattura il giro
completo: documenta che il beccheggio non si ferma a 90 gradi.

## Idee per il prossimo giro

- Modalità a due giocatori in split-screen (il mondo è già indipendente dal
  rendering).
- Terreno distruttibile a voxel al posto delle rocce.
- Classifica locale delle partite migliori.
