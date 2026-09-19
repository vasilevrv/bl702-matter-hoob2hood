#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=../versions.env
source "${PROJECT_ROOT}/versions.env"

DEPS_DIR="${PROJECT_ROOT}/deps"
CHIP_ROOT="${DEPS_DIR}/connectedhomeip"
SDK_ROOT="${DEPS_DIR}/bl_iot_sdk"

clone_locked() {
    local name="$1"
    local url="$2"
    local commit="$3"
    local destination="$4"

    if [[ ! -e "${destination}" ]]; then
        echo "Cloning ${name}..."
        git clone --filter=blob:none "${url}" "${destination}"
    elif [[ ! -d "${destination}/.git" ]]; then
        echo "error: ${destination} exists but is not a Git checkout" >&2
        exit 1
    fi

    local current
    current="$(git -C "${destination}" rev-parse HEAD)"
    if [[ "${current}" != "${commit}" ]]; then
        if [[ -n "$(git -C "${destination}" status --porcelain)" ]]; then
            echo "error: ${name} has local changes and is at the wrong revision" >&2
            exit 1
        fi
        echo "Checking out ${name} ${commit}..."
        git -C "${destination}" fetch --depth=1 origin "${commit}"
        git -C "${destination}" checkout --detach "${commit}"
    fi
}

apply_once() {
    local repository="$1"
    local patch_file="$2"

    if git -C "${repository}" apply --reverse --check "${patch_file}" >/dev/null 2>&1; then
        echo "Patch already applied: $(basename "${patch_file}")"
    elif git -C "${repository}" apply --check "${patch_file}"; then
        echo "Applying $(basename "${patch_file}")..."
        git -C "${repository}" apply "${patch_file}"
    else
        echo "error: cannot apply ${patch_file}; dependency tree has unexpected changes" >&2
        exit 1
    fi
}

mkdir -p "${DEPS_DIR}"
clone_locked "bflb-connectedhomeip" "${CONNECTEDHOMEIP_REPO}" "${CONNECTEDHOMEIP_COMMIT}" "${CHIP_ROOT}"
clone_locked "bl_iot_sdk" "${BL_IOT_SDK_REPO}" "${BL_IOT_SDK_COMMIT}" "${SDK_ROOT}"

echo "Checking out Matter submodules..."
(
    cd "${CHIP_ROOT}"
    python3 scripts/checkout_submodules.py --shallow --recursive --jobs 8 --platform bouffalolab
)

apply_once "${CHIP_ROOT}" "${PROJECT_ROOT}/patches/connectedhomeip.patch"

COMPONENTS_ROOT="${CHIP_ROOT}/third_party/bouffalolab/repo/components"
if [[ ! -d "${COMPONENTS_ROOT}/.git" && ! -f "${COMPONENTS_ROOT}/.git" ]]; then
    echo "error: Bouffalo components submodule was not checked out" >&2
    exit 1
fi
apply_once "${COMPONENTS_ROOT}" "${PROJECT_ROOT}/patches/bouffalolab-components.patch"

APP_DEST="${CHIP_ROOT}/examples/hood-app"
rm -rf "${APP_DEST}"
mkdir -p "${APP_DEST}"
cp -R "${PROJECT_ROOT}/app/hood-app/." "${APP_DEST}/"

ln -s ../../../build_overrides "${APP_DEST}/bouffalolab/bl702/build_overrides"
mkdir -p "${APP_DEST}/bouffalolab/bl702/third_party"
ln -s ../../../../.. "${APP_DEST}/bouffalolab/bl702/third_party/connectedhomeip"

python3 "${APP_DEST}/hood-common/generate_data_model.py"

echo "Dependencies and app overlay are ready."

