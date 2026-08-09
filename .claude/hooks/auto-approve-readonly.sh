#!/bin/bash
# PreToolUse hook: skip the permission prompt for a few read-only shell commands.
#
# Silence (exit 0, no output) means "no opinion" and the normal permission flow
# decides. This hook only ever emits "allow", never "deny", so anything it turns
# down still reaches the usual prompt. It is a convenience filter, not a security
# boundary — keep the allowlist small and keep it read-only.

set -u

cmd=$(jq -r '.tool_input.command // empty')

# No command to judge: a missing field, or a tool other than Bash reached us.
if [ -z "$cmd" ]; then
    exit 0
fi

# A prefix match is only meaningful on a single command. Chaining, substitution,
# redirection, and backgrounding all let a second command ride along behind an
# allowlisted first word, as in `cat notes.txt && rm -rf build`. Reject those
# metacharacters outright rather than attempt to parse the shell.
case "$cmd" in
    *';'* | *'&'* | *'|'* | *'`'* | *'$('* | *'<'* | *'>'* | *$'\n'*) exit 0 ;;
esac

# `git diff --output=FILE` writes a file, as does any other smuggled output path.
case "$cmd" in
    *'--output'*) exit 0 ;;
esac

# Anchored at the start, with a word boundary after the verb so `ls` does not
# also admit `lsof`.
if [[ $cmd =~ ^(ls|cat|echo|pwd|whoami|date|git[[:space:]]+(status|log|diff))([[:space:]]|$) ]]; then
    printf '%s\n' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow","permissionDecisionReason":"Read-only command on the project allowlist"}}'
fi

exit 0
