# PS5 menus and controller controls

## Existing November 24 development build

The retained November 24 PS5 development source uses **L3 + R3 together** to open settings and **L1 / R1** to switch pages. It also retains a separate **L1 + R1 + touchpad** Mods-panel shortcut. These controls describe that development source, not every older store release; check the release notes for your installed version. The separate shortcuts are being consolidated.

## Unified-menu candidate — not yet released or console-tested

The November 25 local candidate keeps the upstream PC menu renderer and integrates PS5 controls into it:

- **L3 + R3 together:** open settings.
- **L1 / R1:** move between Graphics, Controls, Enhancements, System and **Mods**.
- **D-pad:** select a row or change its value; **Cross:** activate; **Circle:** back/resume.
- **Mods** is directly on the main tab bar. No touchpad combination is needed in this candidate.
- **Graphics:** display scaling, FSR and texture options. **Enhancements:** a Category selector for Movies, Effects and Advanced; the existing visual options remain available without one long graphics list.
- **Movies:** blurred borders by default for fresh settings; fullscreen remains selectable. Existing saved preferences take priority.
- **Resume**, **Back**, **Return to title**, and **Exit game** are menu actions. Return to title requires confirmation and is limited to normal gameplay after movies/transitions. Save normal progress before leaving.

Experimental quick-save/checkpoint UI is deferred for this candidate. Normal memory-card saves and existing state files are preserved. Do not delete settings or saves to try the menu.

This is an implementation/test plan with a locally compiled candidate, **not a claim that the store download or console installation has these changes**. Boot, visual layout, settings persistence and clean exit still require exact-build console testing.
