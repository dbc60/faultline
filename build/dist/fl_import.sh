#!/usr/bin/env bash
# See LICENSE.txt for copyright and licensing information about this file.
#
# Import, update, or remove a FaultLine service package in a consumer repo.
# Bash counterpart of fl_import.ps1 -- same manifest/lockfile formats and the
# same reconcile-and-remove behavior. Requires bash 4+ (associative arrays) and
# sha256sum, both available under MSYS2/Git-Bash on Windows.
#
# Usage:
#   fl_import.sh --from <package-dir> [--into <root>] [--no-dep-check]
#   fl_import.sh --remove <service>   [--into <root>]
#   fl_import.sh --list               [--into <root>]
#
#   <package-dir>  directory containing manifest.txt (e.g. dist/memory)
#   --into <root>  consumer vendored tree (default: third_party/faultline)
#
# All services merge into <root>/{src,include}; ownership is tracked in
# <root>/faultline.lock so stale files are deleted on update and shared files
# are reference-counted.

set -euo pipefail

FROM=""
INTO="third_party/faultline"
REMOVE=""
DO_LIST=0
NO_DEP_CHECK=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --from)         FROM="$2"; shift 2 ;;
        --into)         INTO="$2"; shift 2 ;;
        --remove)       REMOVE="$2"; shift 2 ;;
        --list)         DO_LIST=1; shift ;;
        --no-dep-check) NO_DEP_CHECK=1; shift ;;
        -h|--help)
            grep '^#' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

LOCKPATH="$INTO/faultline.lock"

declare -A LOCK_SVC      # service name -> version
LOCK_FILES=()            # entries of the form "svc\tpath"

read_lock() {
    LOCK_SVC=()
    LOCK_FILES=()
    [[ -f "$LOCKPATH" ]] || return 0
    local line kind a b
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line#"${line%%[![:space:]]*}"}"   # left-trim
        [[ -z "$line" || "${line:0:1}" == "#" ]] && continue
        read -r kind a b <<<"$line"
        case "$kind" in
            svc) [[ -n "$a" && -n "$b" ]] && LOCK_SVC["$a"]="$b" ;;
            f)   [[ -n "$a" && -n "$b" ]] && LOCK_FILES+=("$a"$'\t'"$b") ;;
        esac
    done < "$LOCKPATH"
}

write_lock() {
    mkdir -p "$(dirname "$LOCKPATH")"
    {
        echo "# FaultLine import lockfile -- managed by fl_import. Do not edit by hand."
        local name entry s p
        for name in "${!LOCK_SVC[@]}"; do
            echo "svc $name ${LOCK_SVC[$name]}"
            for entry in "${LOCK_FILES[@]:-}"; do
                [[ -z "$entry" ]] && continue
                s="${entry%%$'\t'*}"; p="${entry#*$'\t'}"
                [[ "$s" == "$name" ]] && echo "f $name $p"
            done
        done
    } > "$LOCKPATH"
}

# count owners of a path, excluding a given service
count_other_owners() {
    local path="$1" exclude="$2" entry s p n=0
    for entry in "${LOCK_FILES[@]:-}"; do
        [[ -z "$entry" ]] && continue
        s="${entry%%$'\t'*}"; p="${entry#*$'\t'}"
        [[ "$p" == "$path" && "$s" != "$exclude" ]] && n=$((n + 1))
    done
    echo "$n"
}

# true if (svc,path) is already in the lockfile
svc_owns_path() {
    local svc="$1" path="$2" entry s p
    for entry in "${LOCK_FILES[@]:-}"; do
        [[ -z "$entry" ]] && continue
        s="${entry%%$'\t'*}"; p="${entry#*$'\t'}"
        [[ "$s" == "$svc" && "$p" == "$path" ]] && return 0
    done
    return 1
}

# drop all lockfile entries for a service
drop_svc_files() {
    local svc="$1" entry s kept=()
    for entry in "${LOCK_FILES[@]:-}"; do
        [[ -z "$entry" ]] && continue
        s="${entry%%$'\t'*}"
        [[ "$s" == "$svc" ]] || kept+=("$entry")
    done
    LOCK_FILES=("${kept[@]:-}")
}

prune_empty_dirs() {
    [[ -d "$INTO" ]] || return 0
    find "$INTO" -mindepth 1 -type d -empty -delete 2>/dev/null || true
}

sha() { sha256sum "$1" | cut -d' ' -f1; }

read_lock

# --- List ---------------------------------------------------------------
if [[ $DO_LIST -eq 1 ]]; then
    if [[ ${#LOCK_SVC[@]} -eq 0 ]]; then
        echo "No services imported under $INTO"
        exit 0
    fi
    echo "Imported services under $INTO:"
    for name in "${!LOCK_SVC[@]}"; do
        cnt=0
        for entry in "${LOCK_FILES[@]:-}"; do
            [[ -z "$entry" ]] && continue
            [[ "${entry%%$'\t'*}" == "$name" ]] && cnt=$((cnt + 1))
        done
        printf '  %-18s %-10s %s file(s)\n' "$name" "${LOCK_SVC[$name]}" "$cnt"
    done
    exit 0
fi

# --- Remove -------------------------------------------------------------
if [[ -n "$REMOVE" ]]; then
    if [[ -z "${LOCK_SVC[$REMOVE]:-}" ]]; then
        echo "Service '$REMOVE' is not installed under $INTO"
        exit 0
    fi
    deleted=0
    for entry in "${LOCK_FILES[@]:-}"; do
        [[ -z "$entry" ]] && continue
        s="${entry%%$'\t'*}"; p="${entry#*$'\t'}"
        [[ "$s" == "$REMOVE" ]] || continue
        if [[ "$(count_other_owners "$p" "$REMOVE")" -eq 0 ]]; then
            dst="$INTO/$p"
            if [[ -f "$dst" ]]; then rm -f "$dst"; deleted=$((deleted + 1)); fi
        fi
    done
    drop_svc_files "$REMOVE"
    unset 'LOCK_SVC[$REMOVE]'
    write_lock
    prune_empty_dirs
    echo "Removed '$REMOVE' ($deleted file(s) deleted; shared files kept for other services)."
    exit 0
fi

# --- Import -------------------------------------------------------------
if [[ -z "$FROM" ]]; then
    echo "Specify a package directory with --from, or use --list / --remove." >&2
    exit 2
fi
if [[ ! -d "$FROM" ]]; then
    echo "Package directory not found: $FROM" >&2
    exit 1
fi

MANIFEST="$FROM/manifest.txt"
[[ -f "$MANIFEST" ]] || { echo "No manifest.txt found in package: $FROM" >&2; exit 1; }

M_NAME=""
M_VERSION=""
M_DEPENDS=()
M_PATHS=()
M_HASHES=()
while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line#"${line%%[![:space:]]*}"}"
    [[ -z "$line" || "${line:0:1}" == "#" ]] && continue
    read -r kind rest <<<"$line"
    case "$kind" in
        name)    M_NAME="$rest" ;;
        version) M_VERSION="$rest" ;;
        depends) read -ra M_DEPENDS <<<"$rest" ;;
        f)
            read -r mp mh <<<"$rest"
            M_PATHS+=("$mp")
            M_HASHES+=("$(echo "$mh" | tr 'A-Z' 'a-z')")
            ;;
    esac
done < "$MANIFEST"

[[ -n "$M_NAME" ]]    || { echo "Manifest missing 'name'" >&2; exit 1; }
[[ -n "$M_VERSION" ]] || { echo "Manifest missing 'version'" >&2; exit 1; }

# Dependency check
if [[ $NO_DEP_CHECK -eq 0 && ${#M_DEPENDS[@]} -gt 0 ]]; then
    missing=()
    for d in "${M_DEPENDS[@]}"; do
        [[ -z "$d" ]] && continue
        [[ -z "${LOCK_SVC[$d]:-}" ]] && missing+=("$d")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "Service '$M_NAME' depends on: ${missing[*]}. Import those first, or re-run with --no-dep-check." >&2
        exit 1
    fi
fi

added=0; updated=0; conflicts=0
for i in "${!M_PATHS[@]}"; do
    p="${M_PATHS[$i]}"; h="${M_HASHES[$i]}"
    src="$FROM/$p"
    [[ -f "$src" ]] || { echo "Manifest lists '$p' but it is missing from the package." >&2; exit 1; }
    dst="$INTO/$p"
    mkdir -p "$(dirname "$dst")"

    if [[ -f "$dst" && "$(count_other_owners "$p" "$M_NAME")" -gt 0 ]]; then
        if [[ "$(sha "$dst")" != "$h" ]]; then
            owners=""
            for entry in "${LOCK_FILES[@]:-}"; do
                [[ -z "$entry" ]] && continue
                s="${entry%%$'\t'*}"; ep="${entry#*$'\t'}"
                [[ "$ep" == "$p" && "$s" != "$M_NAME" ]] && owners+="$s "
            done
            echo "WARNING: shared file '$p' differs from the copy owned by [${owners% }]; overwriting with '$M_NAME' version." >&2
            conflicts=$((conflicts + 1))
        fi
    fi

    cp -f "$src" "$dst"
    [[ "$(sha "$dst")" == "$h" ]] || { echo "Hash mismatch after copying '$p' (corrupt package?)." >&2; exit 1; }

    if svc_owns_path "$M_NAME" "$p"; then updated=$((updated + 1)); else added=$((added + 1)); fi
done

# Stale removal: files this service used to ship but no longer does.
removed=0
for entry in "${LOCK_FILES[@]:-}"; do
    [[ -z "$entry" ]] && continue
    s="${entry%%$'\t'*}"; p="${entry#*$'\t'}"
    [[ "$s" == "$M_NAME" ]] || continue
    still=0
    for np in "${M_PATHS[@]}"; do [[ "$np" == "$p" ]] && { still=1; break; }; done
    [[ $still -eq 1 ]] && continue
    if [[ "$(count_other_owners "$p" "$M_NAME")" -eq 0 ]]; then
        dst="$INTO/$p"
        if [[ -f "$dst" ]]; then rm -f "$dst"; removed=$((removed + 1)); fi
    fi
done

# Rewrite this service's ownership in the lockfile.
drop_svc_files "$M_NAME"
for p in "${M_PATHS[@]}"; do
    LOCK_FILES+=("$M_NAME"$'\t'"$p")
done
LOCK_SVC["$M_NAME"]="$M_VERSION"
write_lock
prune_empty_dirs

echo ""
echo "Imported '$M_NAME' v$M_VERSION into $INTO"
echo "  $added added, $updated updated, $removed removed (stale), $conflicts shared-file conflict(s)"
