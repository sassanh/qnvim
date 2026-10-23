---
name: qtc-api-port
description: qnvim auf eine neue Qt-Creator-Version portieren oder eine Qt-Creator-API nachschlagen (IEditor, IDocument, IEditorFactory, EditorManager, ActionManager, Aspects, MIME-Datenbank). Verwenden bei Versionssprüngen, Compile-Fehlern durch API-Änderungen oder wenn unklar ist, wie eine Qt-Creator-Schnittstelle sich verhält.
---

# Qt-Creator-API portieren und nachschlagen

## Grundregeln

- Nur **öffentliche** API (`coreplugin/...`, `texteditor/...`, `utils/...`,
  `projectexplorer/...` Header ohne `_p.h`). Private Header nie einbinden –
  sie sind nicht im qtcreator_dev-Paket. Quellcode lesen ist erlaubt.
- Qt Creator garantiert API-Kompatibilität nur innerhalb von Major.Minor
  (`CompatVersion`, z. B. 20.0.0 für alle 20.0.x). Es gibt keine 20.1/20.2 –
  nach 20.0.x kommt 21.0.

## Quellen besorgen

```sh
git ls-remote --tags https://github.com/qt-creator/qt-creator.git | grep 'v2[0-9]\.'
git clone --depth 1 --branch v<Version> https://github.com/qt-creator/qt-creator.git
```

Die Branding-Datei `cmake/QtCreatorIDEBranding.cmake` nennt `IDE_VERSION` und
`IDE_VERSION_COMPAT`.

## Versionssprung – Checkliste

1. `external/qtcreator/version.cmake` → `QT_CREATOR_VERSION` setzen.
2. Versionsverweise in `README.md`, `AGENTS.md`, `docs/building.md` anpassen.
3. Compile-Check: `tools/compilecheck/run.sh` (Skill `qnvim-build`).
4. Jeden Fehler gegen die neuen Header klären:
   `git -C <qtc> log --oneline v<alt>..v<neu> -- src/plugins/coreplugin/editormanager/ieditor.h`
   zeigt, was sich an einem Header geändert hat.
5. Benötigte Qt-Version prüfen (`IDE_QT_VERSION_MIN` in `CMakeLists.txt` von Qt Creator,
   Qt-Version der Release-Builds aus der Ankündigung).
6. CHANGELOG „Changed: Target Qt Creator X“.

## Wichtige Fakten (Stand 20.0.2), von denen der GUI-Modus abhängt

- **Editorauswahl**: `IEditorFactory::preferredEditorFactories()` sucht Factories
  über die MIME-Hierarchie (spezifischster Typ zuerst, innerhalb einer Ebene in
  Registrierungsreihenfolge) und stellt die vom Benutzer bevorzugte Factory
  (`EditorManager/PreferredEditorFactories` in den Settings, Map MIME → Editor-Id)
  nach vorn. Diese Map liest `ICore::extensionsInitialized()` – also **nach**
  dem `extensionsInitialized()` aller abhängigen Plugins. Darauf beruht
  `NvimGuiSession::takeOverTextEditors()`. Bei jedem Versionssprung prüfen:
  `grep -n preferredEditorFactoriesKey src/plugins/coreplugin/editormanager/editormanager.cpp`
  und die Reihenfolge in `ICore::extensionsInitialized()`.
- **Besitz**: Ein `IEditor` löscht sein Widget und (bei 1 Editor/Dokument) sein
  Dokument selbst (Vorbild: `ImageViewer`). `DocumentModel` löscht Dokumente nur
  für suspendierte Einträge.
- **Speichern**: `IDocument::save()` ruft `saveImpl()`; `aboutToSave`/`saved`
  werden von `save()` gesendet.
- **Neu laden**: `reloadBehavior()` → `BehaviorSilent`, damit Qt Creator nicht
  zusätzlich zu Neovim fragt.
- **Toolbar**: `IEditor::toolBar()` darf `nullptr` liefern (`EditorToolBar` prüft).
- **Aspects**: `Utils::StringAspect::setDisplayStyle(LineEditDisplay)`,
  `SelectionAspect` mit `setUseDataAsSavedValue()`, Layout per `Layouting::`.
- **MIME**: `Utils::allMimeTypes()`, `MimeType::inherits()`, `matchesName()`;
  `mimeutils.h` braucht das Include-Verzeichnis `utils/mimetypes2`.
- **Settings-Keys** sind `Utils::Key` (eigene Klasse, nicht `QString`).

## Antwortformat (für Recherchen)

Fundstelle mit Datei:Zeile, die relevante Signatur, und ob die API öffentlich
(exportiert, nicht `_p.h`) ist. Bei Verhaltensfragen den Codepfad kurz
nachzeichnen, nicht raten.
