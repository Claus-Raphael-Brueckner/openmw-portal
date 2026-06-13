# OpenMW Portal Rendering

An experimental portal rendering mod for [OpenMW](https://openmw.org), implemented directly in C++.

## Motivation

Cell transitions in Morrowind break immersion — walking through a door triggers a loading screen instead of a seamless passage. This mod replaces that with real-time portal rendering: you see the destination through the doorway before you walk through it, and the cell transition happens invisibly as you cross the threshold.

## Status: Pre-Alpha

This is a hobby project with no roadmap and no guarantee of a stable release. It may never be feature-complete. Expect bugs, rough edges, and sudden changes.

What works:
- Portal rendering for most exterior and interior doors (Hlaalu, Imperial, cave entrances, Redoran, Telvanni and others)
- Seamless cell transitions on portal crossing
- Ghost mode when approaching a portal: world collision is temporarily suppressed so cave rocks and door frames don't block the path
- Collision guide shapes (floor ramp and guide cylinders) that funnel the player toward the portal opening
- Water reflections and sky rendering for exterior portals

Known limitations:
- Not all door types are handled correctly
- Sky geometry in exterior portals is a placeholder
- Performance is not optimised
- Some edge cases in rapid cell transitions remain

## Installation

This is a source mod — you need to build OpenMW from source with this branch applied. No binary releases are planned.

```bash
git clone https://github.com/Claus-Raphael-Brueckner/openmw-portal.git
cd openmw-portal
# follow standard OpenMW build instructions
```

## Configuration

Add to your `settings.cfg` (found at `~/.config/openmw/settings.cfg` on Linux):

```ini
[Portal]

# Show magenta wireframe debug geometry for portal collision shapes.
debug geometry = false
```

## License

GPL-3.0, same as OpenMW. See [LICENSE](LICENSE).
