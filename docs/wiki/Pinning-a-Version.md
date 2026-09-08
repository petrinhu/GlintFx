# Pinning a version

This page is for anyone adding glintfx to their own project and wondering:
"which exact copy of glintfx am I building against, and how do I keep it
from changing under me?"

## What "pinning" means

When your build downloads or references glintfx, it has to pick *which*
copy. Without any instruction, most tools default to "whatever the project
looks like right now, on its main branch" - a moving target. **Pinning**
means you write down one exact, unchanging copy instead, so every build
you or your teammates run, today or in six months, uses the same glintfx.

**Example.** Compare these two:

| Without pinning | With pinning |
|---|---|
| "Use glintfx's `main` branch." | "Use glintfx exactly as it was at commit `a1b2c3d...`." |
| Rebuilding next week may get a *different* glintfx, with no warning. | Rebuilding next week gets the *identical* glintfx, guaranteed. |

## What happens if you do not pin

Nothing goes wrong immediately - that is exactly what makes this risky.
Your build keeps working, silently pulling in whatever glintfx's `main`
branch looks like at that moment, until the day it does not:

- A function you called gets a different signature, and your build breaks
  with a compiler error you did not expect from a dependency update.
- A behavior you relied on changes quietly, and your program starts
  misbehaving with no compiler error at all - the harder failure to debug.
- Two teammates building the same project on different days end up with
  two different copies of glintfx, and "it works on my machine" starts
  being literally true.

**glintfx is pre-1.0 today** (see the main README's ["Status"](https://github.com/petrinhu/GlintFx#status-pre-10-under-active-construction)
section): its interface is explicitly allowed to change between commits,
with no advance notice and no deprecation period. Building against it
without pinning is not a small risk here, it is close to guaranteed to
bite you eventually.

## How glintfx numbers its versions

Once glintfx starts tagging releases, each one gets a tag shaped
`vMAJOR.MINOR.PATCH.TWEAK` - four numbers, not the three you may know from
other projects. Each position answers a different question about what
changed:

| Position | Example | Answers |
|---|---|---|
| `MAJOR` | the `1` in `v1.2.3.0` | Did code that used to compile against glintfx stop compiling? |
| `MINOR` | the `2` in `v1.2.3.0` | Was a new feature added, without breaking anything old? |
| `PATCH` | the `3` in `v1.2.3.0` | Was a bug fixed, with no feature added and nothing broken? |
| `TWEAK` | the `0` in `v1.2.3.0` | Did only packaging change (how it is built or installed), with no code difference at all? |

The full rules - including the separate promise about binary compatibility
and about file formats glintfx writes - are in the README's own
["Versioning and compatibility"](https://github.com/petrinhu/GlintFx#versioning-and-compatibility)
section; this page only explains the number, not the whole contract.

## What to pin, right now

**No version has been tagged yet.** There is no `v1.0.0.0`, not even a
`v0.1.0.0`, to reference - the honest statement, not a simplification.
Until the first tag exists, the only working way to pin glintfx is to
reference one exact Git commit, using CMake's `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  glintfx
  GIT_REPOSITORY https://github.com/petrinhu/GlintFx.git
  GIT_TAG        a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2   # <- your real commit here
)
FetchContent_MakeAvailable(glintfx)

target_link_libraries(my_app PRIVATE glintfx::glintfx)
```

To get a real commit to put there, run:

```bash
git ls-remote https://github.com/petrinhu/GlintFx.git main
```

That prints the exact 40-character commit currently at the tip of `main` -
copy it into `GIT_TAG` above. Every time you want to intentionally pick up
newer glintfx changes, you run that command again and update the value
yourself; nothing updates it for you, which is the entire point of
pinning.

**Once glintfx tags its first release**, the same `GIT_TAG` field takes a
tag name (`v1.0.0.0`) instead of a raw commit, and the version-number table
above starts telling you exactly what changed between tags. Until then,
treat every commit as its own, independent, breaking-change-possible
release.

## See also

- [`PACKAGING.md`](https://github.com/petrinhu/GlintFx/blob/main/PACKAGING.md) -
  the full reference for every way to bring glintfx into a project
  (installed, embedded, packaged for a distro).
- [Getting Started](Getting-Started) - if you have not built glintfx at
  all yet, start there first.
