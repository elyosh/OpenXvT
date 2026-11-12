#!/bin/bash

# Signs, notarizes and staples a DMG that build-package.sh already built with
# an ad-hoc signature. The release workflow runs it in its signing job, and a
# maintainer can run it on a Mac. It extracts the application, signs it with
# a Developer ID identity, notarizes and staples the application and the
# rebuilt DMG, and can replace the DMG on a GitHub release.
#
# Usage:
#   sign-package.sh <input-dmg> [output-dmg]
#
# output-dmg defaults to rewriting the input in place. A debug-symbol bundle
# packaged in a Debug DMG is preserved.
#
# Required environment:
#   XVT_MACOS_SIGN_IDENTITY   Developer ID Application identity
#   XVT_MACOS_NOTARY_PROFILE  notarytool keychain profile, or an App Store
#                             Connect API key in XVT_MACOS_NOTARY_KEY (path
#                             to the .p8 file), _KEY_ID and _ISSUER, or an
#                             Apple ID in XVT_MACOS_NOTARY_APPLE_ID, _TEAM_ID
#                             and _PASSWORD
# Optional:
#   XVT_MACOS_RELEASE_TAG     replace the DMG asset on this GitHub release
#                             with `gh release upload --clobber`

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: sign-package.sh <input-dmg> [output-dmg]" >&2
    exit 2
fi

input_dmg="$1"
output_dmg="${2:-${input_dmg}}"

sign_identity="${XVT_MACOS_SIGN_IDENTITY:--}"
notary_profile="${XVT_MACOS_NOTARY_PROFILE:-}"
notary_apple_id="${XVT_MACOS_NOTARY_APPLE_ID:-}"
notary_team_id="${XVT_MACOS_NOTARY_TEAM_ID:-}"
notary_password="${XVT_MACOS_NOTARY_PASSWORD:-}"
notary_key="${XVT_MACOS_NOTARY_KEY:-}"
notary_key_id="${XVT_MACOS_NOTARY_KEY_ID:-}"
notary_issuer="${XVT_MACOS_NOTARY_ISSUER:-}"
release_tag="${XVT_MACOS_RELEASE_TAG:-}"

work_root="$(mktemp -d "${TMPDIR:-/tmp}/openxvt-sign.XXXXXX")"
mount_point="${work_root}/mount"
mounted=0

# Globals consumed by the shared helpers.
application="${work_root}/OpenXvT.app"
artifact_dir="$(dirname "${output_dmg}")"
dmg="${output_dmg}"
dmg_volume_name="OpenXvT"
dmg_extras="${script_dir}/dmg"

# Shared signing, notarization, and DMG helpers.
source "${script_dir}/common.sh"

cleanup() {
    if [[ "${mounted}" -eq 1 ]]; then
        hdiutil detach "${mount_point}" -force >/dev/null 2>&1 || true
    fi
    rm -rf "${work_root}"
}

trap cleanup EXIT

extract_application() {
    mkdir -p "${mount_point}"
    hdiutil attach "${input_dmg}" -readonly -nobrowse \
        -mountpoint "${mount_point}"
    mounted=1

    if [[ ! -d "${mount_point}/OpenXvT.app" ]]; then
        echo "No OpenXvT.app found in ${input_dmg}" >&2
        exit 1
    fi

    ditto "${mount_point}/OpenXvT.app" "${application}"
    if [[ -d "${mount_point}/OpenXvT.dSYM" ]]; then
        ditto "${mount_point}/OpenXvT.dSYM" "${work_root}/OpenXvT.dSYM"
        dsym_bundle="${work_root}/OpenXvT.dSYM"
    fi
    hdiutil detach "${mount_point}"
    mounted=0
}

main() {
    for command in codesign ditto file hdiutil spctl xcrun; do
        require_command "${command}"
    done

    resolve_notary_arguments

    if [[ "${sign_identity}" == "-" ]]; then
        echo "XVT_MACOS_SIGN_IDENTITY must name a Developer ID identity" >&2
        exit 1
    fi
    if [[ "${notarize_enabled}" -ne 1 ]]; then
        echo "Notarization credentials are required: set" >&2
        echo "XVT_MACOS_NOTARY_PROFILE, the XVT_MACOS_NOTARY_KEY variables," >&2
        echo "or the XVT_MACOS_NOTARY_APPLE_ID variables" >&2
        exit 1
    fi
    if [[ -n "${release_tag}" ]]; then
        require_command gh
    fi
    if [[ ! -f "${input_dmg}" ]]; then
        echo "Input disk image not found: ${input_dmg}" >&2
        exit 1
    fi

    extract_application
    sign_bundle
    notarize_application
    create_dmg
    notarize_dmg

    if [[ -n "${release_tag}" ]]; then
        gh release upload "${release_tag}" "${output_dmg}" --clobber
        echo "Replaced $(basename "${output_dmg}") on release ${release_tag}"
    fi

    echo "Created ${output_dmg}"
}

main "$@"
