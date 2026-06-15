#!/usr/bin/env bash
# Build DACE-capable veriumd.exe and vericoind.exe for the Vericonomy wallet.
#
# On WSL + Windows checkout: syncs sources to ~/vericonomy-build (native ext4 —
# autotools/depends fail on /mnt/c), builds depends for MinGW, then cross-
# compiles both daemons and copies the .exe files back into vericoin/src/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
BUILD_DIR="${DACE_BUILD_DIR:-$HOME/vericonomy-build}"
CROSS_HOST="x86_64-w64-mingw32"
EXE_SUFFIX=".exe"
LOG_FILE="${DACE_BUILD_LOG:-$BUILD_DIR/build-dace.log}"

need() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing required tool: $1" >&2
    exit 1
  }
}

log() {
  mkdir -p "$(dirname "$LOG_FILE")"
  echo "$@" | tee -a "$LOG_FILE"
}

need autoreconf
need make
need rsync
need "${CROSS_HOST}-g++"
need "${CROSS_HOST}-windres"

# OpenSSL's mingw64 build invokes bare `windres`; Ubuntu only ships the
# prefixed tool (x86_64-w64-mingw32-windres). Provide a wrapper with headers.
setup_mingw_path() {
  local wrap="${BUILD_DIR}/.mingw-wrap"
  mkdir -p "$wrap"
  cat > "${wrap}/windres" <<EOF
#!/bin/sh
exec ${CROSS_HOST}-windres \\
  -I/usr/x86_64-w64-mingw32/include \\
  -I/usr/share/mingw-w64/include \\
  "\$@"
EOF
  chmod +x "${wrap}/windres"
  export PATH="${wrap}:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
}

normalize_autotools() {
  local dir="$1"
  log "==> Normalizing CRLF (Windows checkout) under ${dir}"
  find "$dir" -type f \( \
    -name 'Makefile' -o -name '*.mk' -o -name '*.am' -o -name '*.ac' -o \
    -name '*.in' -o -name '*.m4' -o -name '*.sh' -o -name 'config.guess' -o \
    -name 'config.sub' -o -name '*.h' -o -name '*.cpp' -o -name '*.c' \
  \) -not -path '*/.git/*' -exec sed -i 's/\r$//' {} + 2>/dev/null || true
  chmod +x "$dir/depends/config.guess" "$dir/depends/config.sub" 2>/dev/null || true
  chmod +x "$dir/autogen.sh" "$dir/build-dace.sh" 2>/dev/null || true
}

sync_sources() {
  log "==> Syncing sources to ${BUILD_DIR} (native WSL filesystem)"
  mkdir -p "$(dirname "$BUILD_DIR")"
  rsync -a --delete \
    --exclude '.git' \
    --exclude 'depends/work' \
    --exclude 'depends/built' \
    --exclude 'autom4te.cache' \
    "$ROOT/" "$BUILD_DIR/"
  normalize_autotools "$BUILD_DIR"
}

prepare_autotools() {
  cd "$BUILD_DIR"
  rm -rf autom4te.cache src/univalue/autom4te.cache 2>/dev/null || true
  rm -f configure Makefile.in src/univalue/configure src/univalue/Makefile.in
  chmod +x autogen.sh build-dace.sh 2>/dev/null || true
  ./autogen.sh
}

build_depends() {
  cd "$BUILD_DIR/depends"
  log "==> Building depends for ${CROSS_HOST} (first run can take 30+ minutes)"
  log "    NO_QT=1 NO_QR=1 — headless daemons only (Qt 5.9.8 fails on GCC 13)."
  setup_mingw_path
  make HOST="${CROSS_HOST}" NO_QT=1 NO_QR=1 -j"${JOBS}"
}

clean_tree() {
  cd "$BUILD_DIR"
  if [[ -f Makefile ]]; then
    log "==> make distclean (switching client / fresh configure)"
    make distclean >/dev/null 2>&1 || make distclean || true
  fi
  rm -rf autom4te.cache src/.deps src/*.o src/qt/.deps 2>/dev/null || true
}

configure_client() {
  local enable_flag="$1"
  setup_mingw_path
  log "==> configure ${enable_flag} (${CROSS_HOST})"
  CONFIG_SITE="$BUILD_DIR/depends/${CROSS_HOST}/share/config.site" \
    ./configure "${enable_flag}" --prefix=/ \
      --host="${CROSS_HOST}" \
      --without-gui --disable-tests --disable-bench \
      --with-incompatible-bdb
}

make_daemon() {
  local binary="$1"
  local target="${binary}${EXE_SUFFIX}"
  log "==> make -C src ${target} (-j${JOBS})"
  make -j"${JOBS}" -C src "${target}"
  if [[ ! -f "src/${target}" ]]; then
    echo "ERROR: expected src/${target} after build" >&2
    echo "Hint: MinGW targets are ${binary}.exe, not ${binary}." >&2
    exit 1
  fi
  echo "==> built $(realpath "src/${target}")"
  cp -f "src/${target}" "$ROOT/src/${target}"
  log "==> copied to $ROOT/src/${target}"
}

build_client() {
  local enable_flag="$1"
  local binary="$2"
  cd "$BUILD_DIR"
  clean_tree
  configure_client "${enable_flag}"
  make_daemon "${binary}"
}

preflight() {
  mkdir -p "$BUILD_DIR"
  : >"$LOG_FILE"
  log "==> DACE sidecar build"
  log "    source:  ${ROOT}"
  log "    build:   ${BUILD_DIR}"
  log "    host:    ${CROSS_HOST}"
  log "    jobs:    ${JOBS}"
  log "    log:     ${LOG_FILE}"
}

if [[ "$ROOT" == /mnt/* ]]; then
  preflight
  log "==> Windows checkout detected — building MinGW sidecars via depends"
  sync_sources
  prepare_autotools
  build_depends
  build_client --enable-verium veriumd
  build_client --enable-vericoin vericoind
else
  preflight
  log "==> Native build (non-Windows checkout)"
  normalize_autotools "$ROOT"
  BUILD_DIR="$ROOT"
  EXE_SUFFIX=""
  cd "$BUILD_DIR"
  if [[ ! -f ./configure ]]; then
    ./autogen.sh
  fi
  build_client --enable-verium veriumd
  build_client --enable-vericoin vericoind
fi

log ""
log "DACE daemons ready:"
log "  $ROOT/src/veriumd${EXE_SUFFIX}"
log "  $ROOT/src/vericoind${EXE_SUFFIX}"
log ""
log "Install into the wallet sidecar directory:"
log "  cd ../verium/desktop/verium-app && npm run fetch:sidecars:dace"
