{
  lib,
  stdenvNoCC,
  arduino-cli,
  cacert,
}:

stdenvNoCC.mkDerivation {
  pname = "arduino-esp32-core-data";
  version = "3.3.8";

  dontUnpack = true;

  nativeBuildInputs = [
    arduino-cli.pureGoPkg
    cacert
  ];

  buildPhase = ''
    runHook preBuild

    export HOME="$TMPDIR/home"
    export ARDUINO_DIRECTORIES_DATA="$out/share/arduino"
    export ARDUINO_DIRECTORIES_DOWNLOADS="$TMPDIR/downloads"
    export ARDUINO_DIRECTORIES_USER="$TMPDIR/user"

    mkdir -p \
      "$HOME" \
      "$ARDUINO_DIRECTORIES_DATA" \
      "$ARDUINO_DIRECTORIES_DOWNLOADS" \
      "$ARDUINO_DIRECTORIES_USER"

    arduino-cli config init
    arduino-cli config add \
      board_manager.additional_urls \
      https://espressif.github.io/arduino-esp32/package_esp32_index.json
    arduino-cli core update-index
    arduino-cli core install esp32:esp32@3.3.8

    # Board Manager's download cache and installation identity are mutable
    # runtime state.  They are deliberately excluded from the Nix output.
    rm -rf "$ARDUINO_DIRECTORIES_DATA/staging" "$ARDUINO_DIRECTORIES_DATA/tmp"
    rm -f "$ARDUINO_DIRECTORIES_DATA/inventory.yaml"
    mkdir -p "$ARDUINO_DIRECTORIES_DATA/staging"

    runHook postBuild
  '';

  installPhase = "true";

  outputHashMode = "recursive";
  outputHashAlgo = "sha256";
  outputHash = "sha256-NwnAbxeAcpQW7ZKA/GBPb1cpI+c4Kv1GGZ03PmJ9UHI=";

  meta = {
    description = "Pinned Arduino ESP32 core and toolchain data for PRTSN";
    homepage = "https://github.com/espressif/arduino-esp32";
    license = lib.licenses.lgpl21Plus;
    platforms = [
      "x86_64-linux"
      "aarch64-linux"
    ];
  };
}
