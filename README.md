# TacticalDisplay

Reusable Dear ImGui/Win32/D3D11 presentation library with domain-provider injection. Includes pan/zoom, unit/contact selection, uncertainty radii, details, available routes/operations, status and controls. Control submissions must run away from rendering and be drained before Display destruction. Its independent Development application uses local providers only.

Architecture, development policy and evidence live in [Docs](../Docs/ARCHITECTURE.md). Read [contributor instructions](../Docs/AGENTS.md), [module structure](../Docs/MODULE_STRUCTURE.md), [local source overrides](../Docs/LOCAL_SOURCES.md) and [code style](../Docs/CODE_STYLE.md) before changes. Current validation and live deployment procedures are in [the integration report](../Docs/Tasks/SeaPower-TacticalDisplay/IMPLEMENTATION.md).

Build from a VS x64 developer shell with CMake 4.2+, Ninja, and VCPKG_ROOT configured:

```powershell
cmake --preset windows-msvc-release -DFETCHCONTENT_SOURCE_DIR_NCBUILD=/absolute/path/to/Build.CMake
cmake --build o/windows-msvc-release --parallel 8
ctest --test-dir o/windows-msvc-release --output-on-failure
```

Internal dependencies use shared Build.CMake and optional ignored Build.Local.cmake registrations. Windows uses x64-windows-static and static MSVC CRT. Generated artifacts stay under o/. Managed builds additionally require the configured game/reference context; see Docs/MANAGED_BUILD.md.

## NATO presentation

The map defaults to filled dark NATO symbols. The Style selector switches between
filled and unfilled dark presets through `Symbology.Nato.Api::SymbolStyle`.
Scale, label frequency, type-only/all-label richness and speed-leader seconds
remain independent controls in a responsive table with bounded input widths and
visible separators.

Screen-space decluttering keeps individual objects separate using the injected
renderer’s measured icon/modifier/visible-label bounds. Hover fixes the displayed
icon while its labels expand. Selection alone does not affect layout order or
invalidate an unchanged solution; label visibility changes still require layout.
Only displaced icons expose exact-position dots and subdued dashed connectors.
Layout reserves space around geographic markers, including coincident markers,
so neighboring icons and labels do not cover them when a placement is feasible. Navigation, speed leaders and uncertainty remain
geographically anchored.

The deterministic search is bounded. In overconstrained viewports labels can clip
and symbols can still overlap; objects remain in the tactical list and no clusters
or aggregate symbols are created. Session and perspective changes discard layout
state. Ordinary pan and zoom reuse offsets and allow symbols to return to their
actual anchors as room becomes available. After geometry or hover changes, layout
searches from the geographic anchor rather than accepting an inflated old offset
after scale or label visibility changes. During ordinary object motion, spatial
hysteresis retains a clear placement unless the displacement improvement exceeds
an eight-pixel deadband plus one quarter of the relocation distance. Collision
repairs prefer nearby positions and leave four pixels of extra clearance. This
prevents small position/label changes from repeatedly swapping nearby symbols.

`NavalCommand.TacticalDisplay.Test` covers layout, hover, hit testing and rendering
integration. The visual smoke fixture writes `symbol-styles.ppm` and
`measured-scene.ppm` and `platform-icons.ppm` in its CTest working directory,
using real ImGui geometry and font-atlas pixels without a GPU or Sea Power session.
