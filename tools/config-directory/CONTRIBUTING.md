# Changing the catalogue

How a setup is added, corrected or removed in
[cod2unbound/cod2-configs](https://github.com/cod2unbound/cod2-configs). The tools
are in the CoD2 Unbound repository under `tools/config-directory`; the
commands below are run from a checkout of it, with `CATALOGUE` set to a
checkout of cod2-configs and `SCRATCH` to an empty folder outside both.

Every change ends the same way: generate, check, sign, push. Only the
maintainer can sign, so a pull request stops after the check and the
maintainer does the rest.

## What may be listed

- A config that was published on the open web, with the page it came from,
  or one its player shared from the app (see below). Nothing from a private
  message or a paid download, and nothing from a player's disk that the
  player did not send themselves.
- Credited to the name the page gives, or for a shared setup the name the
  player typed. Say `self-published` only when the player posted or shared it
  themselves; otherwise `listed` or `uploaded`.
- No real names, nationalities, birthdays or photos, even when the page
  shows them.

## Build the tools once

```sh
sh tools/config-directory/build-audit.sh "$PWD" "$SCRATCH"
```

This writes `clean`, `audit` and `sign` to `$SCRATCH`, with a copy of the
launcher's `ConfigCatalog.json`.

## Add a setup

1. Clean the published text. `clean` runs the launcher's own import review
   for both game versions and leaves out every line it blocks and every line
   that says who the player is, what they typed, a secret, a path or a
   setting of another game. The lines that remain keep their order and bytes.

   ```sh
   mkdir "$CATALOGUE/entries/<slug>"
   "$SCRATCH/clean" published.cfg "$SCRATCH/ConfigCatalog.json" "$CATALOGUE/entries/<slug>/config.cfg"
   ```

   Read the result. Do not edit it by hand: if a line should not be there,
   the review is what needs fixing.

2. Write `entries/<slug>/entry.json` to `entry.schema.json`: the credit, the
   `sourceURL` (https), and `provenance` with the page's name, the day the
   copy was taken, how the name got on it, and a sentence on what was left
   out. `facts` holds the sensitivity, frame cap and resolution as the config
   writes them.

3. Generate, with the next revision number. The revision only ever goes up:
   the launcher refuses an index older than the one it has, and refuses
   changed bytes at a revision it has already seen.

   ```sh
   python3 tools/config-directory/generate.py --root "$CATALOGUE" \
     --base-url https://raw.githubusercontent.com/cod2unbound/cod2-configs/main/ \
     --revision <last revision + 1> --audit-binary "$SCRATCH/audit"
   ```

   Generation fails if either game version's review blocks anything in a
   config. `review.json` records how many lines each review calls safe and
   how many it asks about.

4. Check. This compares every generated file byte for byte.

   ```sh
   python3 tools/config-directory/generate.py --root "$CATALOGUE" \
     --base-url https://raw.githubusercontent.com/cod2unbound/cod2-configs/main/ \
     --revision <the same number> --audit-binary "$SCRATCH/audit" --check
   ```

5. Sign (maintainer only), then commit everything and push.

   ```sh
   sh tools/config-directory/sign-index.sh "$CATALOGUE" <private key file>
   ```

   The key is kept outside every repository. `index.pub` must not change: the
   launcher is built with it, so a different key means no released launcher
   accepts the catalogue.

## A setup shared from the app

From version 0.4.1 a player can share a setup from CoD2 Unbound. The app
cleans the config with the same import review as `clean`, and the project's
website opens a pull request labelled `shared`. It adds `entries/<slug>/`
with two files, in two commits: the setup, then one that points
`sourceURL` at the pull request.

- `config.cfg` is exactly what the app sent. The website refuses anything but
  settings and key binds, and the settings for hosting, the player's name,
  passwords, chat, server addresses, `exec`, `wait` and `quit`, but it does
  not run the review: generation does.
- `entry.json` is credited to the name the player typed (`credit` and
  `player`), `self-published`, `publishedBy` `CoD2 Unbound`, platform
  `native`, retrieved the day it was sent, with a `changes` sentence made from
  the lines the app left out. `era`, `feel`, `story` and `styleTags` are
  written from the config alone, in the words the other entries use.
- The pull request's body says which app version sent it, what was left out
  and the config's SHA-256. Nothing about the sender.

To list it:

1. Read `config.cfg`. Do not edit it: the player's withdraw token is made
   from its bytes, and a line that should not be there means the review needs
   fixing. If it should not be listed, close the pull request with a short
   reason and delete its branch.
2. Edit `entry.json` on the pull request's branch: `era`, `feel`, `story`
   and `styleTags` should read like the other entries. Keep `credit`,
   `player`, `provenance` and `sourceURL`. A name that is a real name, an
   insult or someone else's is a reason to close it, not to rename it.
3. Generate with the next revision and check, as in steps 3 and 4 above,
   then sign (step 5) and commit the generated files to the branch.
4. Merge.

At most 30 shared pull requests are open at once and 40 arrive in a day;
past that the website answers that sharing is closed for the day.

## When a player withdraws a shared setup

Stop Sharing in the app reaches the website, which checks the player's token
against the pull request's own `config.cfg`.

- An open pull request is closed with a comment and its branch deleted.
  Nothing to do.
- A merged setup gets a pull request labelled `withdrawn` that deletes
  `entries/<slug>/`. Treat it like a removal request: generate with the next
  revision, check, sign, merge. Do not add the setup back from another source
  under the same name.

## Correct a setup

Edit its `entry.json`, or clean the published text again for `config.cfg`,
then steps 3 to 5 with a new revision.

## Remove a setup

Delete its folder under `entries/`, then steps 3 to 5 with a new revision.
Launchers drop it the next time they fetch the index. A copy a player has
already imported stays on their Mac; that is theirs.

## When someone asks for a removal or a correction

Requests come as issues on cod2-configs.

- Do it. No reason is needed and none is asked for, and nobody has to prove
  who they are: a wrong removal costs one entry, a refused one costs a
  person their say over their own name.
- Remove or correct within the same change, sign and push, then answer the
  issue with the revision that carries it and close it.
- If the issue itself holds something private (a real name, an address),
  say so to the person and delete the text from the issue after acting.
- Do not add the setup back later from another page under the same name.
