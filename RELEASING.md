# Releasing branch UF2s

Each Vector Raid release contains a separately named UF2 for every maintained
branch in `release-branches.txt`.

The initial branches are:

- `stable`: `codex/create-native-rail-shooter-game-firmware`, the preserved
  known-good game
- `next`: `codex/vector-raid-next`, where new features are developed

## Add another branch

Create and push the branch, then add one line to `release-branches.txt`:

```text
branch-label  full/git-branch-name
```

The label is used in the release asset name and must be unique. Use lowercase
letters, digits, and hyphens. Commit and push the updated branch list before
making a release tag.

## Make a release

From a clean checkout containing the release workflow and branch list:

```sh
git fetch origin
git tag -a vector-raid-vYYYYMMDD -m "Vector Raid YYYY-MM-DD"
git push origin vector-raid-vYYYYMMDD
```

Pushing a `vector-raid-v*` tag starts the GitHub Actions release workflow. At
the beginning of the run it resolves every configured branch to a commit SHA,
builds `fruitjam_railshooter` from each SHA, and creates one GitHub release with
assets such as:

```text
fruitjam-vector-raid-stable-1a2b3c4.uf2
fruitjam-vector-raid-next-5d6e7f8.uf2
SHA256SUMS.txt
BUILD-PROVENANCE.txt
```

The provenance file records the exact branch and commit used for every UF2.
If any configured branch is missing or fails to build, the release is not
created.
