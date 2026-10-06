# Release standards

How this library is versioned, and what a release consists of. Written down 2026-10; the older
sections of `CHANGELOG.md` predate it and were left as they were.

## The changelog

`CHANGELOG.md` is kept by hand.

- Work that is **committed and not yet released** is listed under `### [Unreleased]`, which sits
  at the top of the file. Work that is still in the working tree is not listed — this library is
  developed in one tree, so the entry is written when the commit is made, not before.
- Entries keep the style of the older sections: one bullet, beginning with `New:` / `Changed:` /
  `Fixed:` / `Removed:`, naming the module and saying what changed for whoever uses it.
- On a release the `### [Unreleased]` heading is **renamed** to the version it becomes
  (`### v3.8.0`). It is not copied; the heading itself moves.

## The release commit

A release is a single commit that contains nothing but the two things that claim the release:

| File | What changes |
| --- | --- |
| `CHANGELOG.md` | `### [Unreleased]` becomes the version heading |
| `CMakeLists.txt` | the version in `project(... VERSION x.y.z)` |

It is titled `chore: release x.y.z` (**this replaced the older "bump version"**), it changes no
code, and the tag — `v3.8.0`, same number — is put on it. If something still needs fixing, that
goes in its own commit first; the release commit is not the place to slip it in.

The tree has to build and the tests have to pass *before* the release commit, not after it. A
release commit does not itself imply a build: nothing is recompiled just because the version
changed, but a tag that was never built is a tag nobody can trust.

## Version numbers

`MAJOR.MINOR.PATCH`, read the usual way:

| | When |
| --- | --- |
| MAJOR | a change that breaks the API or ABI of a module that is no longer in early development |
| MINOR | new modules, new features, changes in behaviour |
| PATCH | fixes that leave the API as it is |

Standalone modules carry their own `version:` in their `[SCL_STANDALONE_MODULE]` block (see
[standalone_module.md](standalone_module.md)) and move on their own schedule — a module's version
and the library's version are unrelated. A module change is still worth a changelog entry,
because that is what a reader of the library's history is looking for.

## Not part of this

- Release notes beyond the changelog: there is no separate document to keep in sync.
- Binary artefacts: this library is source, and the installed prefix is a build output, not a
  release.
