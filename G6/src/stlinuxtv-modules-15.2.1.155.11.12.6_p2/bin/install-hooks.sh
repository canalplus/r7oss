#!/bin/bash
HOOK_NAMES="pre-commit pre-applypatch"
# assuming the script is in a bin directory, one level into the repo
HOOK_DIR=../.git/hooks

# Remove the hook folder coming from repo
rm $HOOK_DIR
mkdir $HOOK_DIR

for hook in $HOOK_NAMES; do
    # create the symlink
    ln -s -f ../../bin/githooks/$hook $HOOK_DIR/$hook
done
