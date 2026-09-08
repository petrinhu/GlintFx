# glintfx wiki

Welcome. This wiki is written for someone who is new to computing in general,
not only new to glintfx: every technical word is explained the first time it
is used, and nothing here assumes you have built C++ software before.

## What glintfx is, in plain language

glintfx is a **library**: a piece of software that does not run by itself,
but that another program can use as a building block. Think of it like a
box of tools a carpenter buys instead of forging from scratch - the
carpenter still builds the actual piece of furniture, but does not have to
also invent the hammer and the saw.

Concretely, glintfx aims to give a 2D game or app all of the following, so
that whoever builds on top of it only has to write their own game or app
logic: a window on the screen, a way to keep drawing new frames over time
(the "main loop"), 2D drawing, keyboard/mouse/gamepad input, sound, text,
loading files, and 2D math (positions, sizes, rotations).

**Not all of that exists yet.** glintfx is version **0.x** (see
[Concepts](Concepts) for what that number means), still being built one
piece at a time. The [main README](https://github.com/petrinhu/GlintFx#status-pre-10-under-active-construction)
states, with a date attached, exactly what is real today versus what is
only planned - read that section before assuming a feature exists.

## Where to go from here

- **[Getting Started](Getting-Started)** - how to get a copy of the source
  code onto your own computer, turn it into a working program (this is
  called "building" or "compiling"), and see it work for the first time.
- **[Concepts](Concepts)** - the handful of ideas and words (library vs.
  application, what a "build system" does, what the AGPL license asks of
  you) that make the rest of this project's documentation make sense.
- **[Pinning a Version](Pinning-a-Version)** - how to lock your own
  project to one exact, unchanging copy of glintfx, and what breaks if
  you skip this step. Read this before glintfx becomes a real dependency
  of anything you ship.
- **[README](https://github.com/petrinhu/GlintFx#readme)** - the
  authoritative, always-current statement of what glintfx is, what state
  it is in, which operating systems it targets, and how to consume it from
  your own project once you are past your first build.
- **[docs/api-conventions.md](https://github.com/petrinhu/GlintFx/blob/main/docs/api-conventions.md)** -
  once you are ready to actually call glintfx's functions from your own
  code, this explains the one pattern every function that can fail uses to
  report that failure.

### API reference

What every function and type does today, area by area, each with a
working example. Only the areas that are complete and stable are listed
here - see the main README's ["Status"](https://github.com/petrinhu/GlintFx#status-pre-10-under-active-construction)
section for what is still being built.

- **[API Reference: Core](API-Core)** - error handling, versioning, 2D
  math, color and time. Start here: almost everything else builds on the
  error-handling pattern this page explains first.
- **[API Reference: Window and Display](API-Window-and-Display)** - the
  one area that is complete and tested on both Linux and Windows today:
  connecting to the display and opening a real window.
- **[API Reference: Node View](API-Node-View)** - the one contract from
  the future style engine that is already public and stable: the table
  of callbacks your own UI tree implements so glintfx can inspect it.

## A word about the license, before you invest time

glintfx is licensed under the **GNU Affero General Public License v3.0 or
later (AGPL-3.0-or-later)**. In short, and in plain language: you can use,
study, and modify glintfx freely, but if you distribute a program built
with it (including running it as a network service others connect to), you
must also make the complete source code of your own program available
under the same license. This is a real, meaningful obligation for
commercial or closed-source use, not a formality - read the
[full license text](https://github.com/petrinhu/GlintFx/blob/main/LICENSE)
and the README's own "License" section before deciding to build on
glintfx, especially if what you are building will not itself be open
source.
