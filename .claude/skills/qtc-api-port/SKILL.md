---
name: qtc-api-port
description: Port qnvim to a new Qt Creator version or look up a Qt Creator API (IEditor, IDocument, IEditorFactory, EditorManager, ActionManager, Aspects, MIME database). Use for version bumps, compile errors caused by API changes, or when it is unclear how a Qt Creator interface behaves.
---

# Porting and looking up the Qt Creator API

## Basic rules

- **Public** API only (`coreplugin/...`, `texteditor/...`, `utils/...`,
  `projectexplorer/...` headers without `_p.h`). Never include private headers —
  they are not in the qtcreator_dev package. Reading the source is allowed.
- Qt Creator guarantees API compatibility only within a major.minor
  (`CompatVersion`, e.g. 20.0.0 for all 20.0.x). There is no 20.1/20.2 — 21.0
  follows 20.0.x.

## Getting the sources

```sh
git ls-remote --tags https://github.com/qt-creator/qt-creator.git | grep 'v2[0-9]\.'
git clone --depth 1 --branch v<version> https://github.com/qt-creator/qt-creator.git
```

The branding file `cmake/QtCreatorIDEBranding.cmake` names `IDE_VERSION` and
`IDE_VERSION_COMPAT`.

## Version bump — checklist

1. Set `QT_CREATOR_VERSION` in `external/qtcreator/version.cmake`.
2. Adjust version references in `README.md`, `AGENTS.md`, `docs/building.md`.
3. Compile check: `tools/compilecheck/run.sh` (skill `qnvim-build`).
4. Investigate each error against the new headers:
   `git -C <qtc> log --oneline v<old>..v<new> -- src/plugins/coreplugin/editormanager/ieditor.h`
   shows what changed in a header.
5. Check the required Qt version (`IDE_QT_VERSION_MIN` in Qt Creator's
   `CMakeLists.txt`; Qt version of the release builds from the announcement).
6. CHANGELOG "Changed: Target Qt Creator X".

## Important facts (as of 20.0.2) that GUI mode depends on

- **Editor selection**: `IEditorFactory::preferredEditorFactories()` searches
  factories through the MIME hierarchy (most specific type first, within a level
  in registration order) and moves the user's preferred factory
  (`EditorManager/PreferredEditorFactories` in the settings, map MIME → editor id)
  to the front. This map is read by `ICore::extensionsInitialized()` — i.e.
  **after** `extensionsInitialized()` of all dependent plugins.
  `NvimGuiSession::takeOverTextEditors()` relies on this. Check on every version
  bump: `grep -n preferredEditorFactoriesKey src/plugins/coreplugin/editormanager/editormanager.cpp`
  and the order in `ICore::extensionsInitialized()`.
- **Ownership**: an `IEditor` deletes its widget and (with 1 editor/document) its
  document itself (model: `ImageViewer`). `DocumentModel` deletes documents only
  for suspended entries.
- **Saving**: `IDocument::save()` calls `saveImpl()`; `aboutToSave`/`saved` are
  emitted by `save()`.
- **Reload**: `reloadBehavior()` → `BehaviorSilent` so Qt Creator does not ask in
  addition to Neovim.
- **Toolbar**: `IEditor::toolBar()` may return `nullptr` (`EditorToolBar` checks).
- **Aspects**: `Utils::StringAspect::setDisplayStyle(LineEditDisplay)`,
  `SelectionAspect` with `setUseDataAsSavedValue()`, layout via `Layouting::`.
- **MIME**: `Utils::allMimeTypes()`, `MimeType::inherits()`, `matchesName()`;
  `mimeutils.h` needs the include directory `utils/mimetypes2`.
- **Settings keys** are `Utils::Key` (its own class, not `QString`).

## Answer format (for research)

Location with file:line, the relevant signature, and whether the API is public
(exported, not `_p.h`). For behavior questions, trace the code path briefly;
don't guess.
