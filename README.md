# Synth Visualizer (VST3)

Audio-Visualizer-Plugin mit fünf Modi: Synthwave, Spectrum, Oscilloscope, Radial, Particles.
Das Audio wird unverändert durchgereicht, das Plugin zeigt nur an, was durch seine Instanz läuft.

## Bauen mit GitHub (ohne lokale Installation)
1. Neues Repository auf github.com anlegen.
2. Alle Dateien aus diesem Ordner hochladen ("Add file" -> "Upload files").
3. Falls der Ordner `.github` nicht mit hochgeladen wurde (versteckt, v. a. auf dem Mac):
   "Add file" -> "Create new file", als Namen `.github/workflows/build.yml` eingeben
   und den Inhalt aus `build-workflow.yml` einfügen.
4. Im Reiter "Actions" den Build abwarten (5-15 Min.), dann unten unter "Artifacts" das Paket für dein System laden.

## Installieren
- Windows: Ordner `Synth Visualizer.vst3` nach `C:\Program Files\Common Files\VST3`
- macOS: `Synth Visualizer.vst3` nach `/Library/Audio/Plug-Ins/VST3`
  (bei Sperre durch Gatekeeper im Terminal: `xattr -cr "/Library/Audio/Plug-Ins/VST3/Synth Visualizer.vst3"`)
- Danach in der DAW einen Plugin-Scan starten.

## Lizenz-Hinweis
Das Plugin nutzt das JUCE-Framework (AGPLv3 oder kommerzielle Lizenz). Für den privaten Gebrauch ist das unproblematisch.
