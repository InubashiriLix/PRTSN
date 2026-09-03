{
  description = "PRTSN firmware development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      supportedSystems = [ "x86_64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    rec {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          esp32ArduinoCore = pkgs.callPackage ./nix/esp32-arduino-core.nix { };
        });

      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          python = pkgs.python3.withPackages (pythonPackages: [
            pythonPackages.pillow
          ]);
          esp32ArduinoCore = packages.${system}.esp32ArduinoCore;
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              arduino-cli
              arduino-language-server
              cargo
              clang-tools
              gnumake
              jq
              pkg-config
              rustc
              python
            ];

            buildInputs = with pkgs; [
              dbus
            ];

            ARDUINO_NIX_DATA = "${esp32ArduinoCore}/share/arduino";
            ARDUINO_ESP32_TOOLS = "${esp32ArduinoCore}/share/arduino/packages/esp32/tools";

            shellHook = ''
              export ARDUINO_DIRECTORIES_DATA="$PWD/.cache/arduino-cli/data"
              export ARDUINO_DIRECTORIES_DOWNLOADS="$PWD/.cache/arduino-cli/downloads"
              export ARDUINO_DIRECTORIES_USER="$PWD/.cache/arduino-cli/user"
              mkdir -p \
                "$ARDUINO_DIRECTORIES_DATA" \
                "$ARDUINO_DIRECTORIES_DOWNLOADS" \
                "$ARDUINO_DIRECTORIES_USER"
              ln -sfn "$ARDUINO_NIX_DATA/packages" "$ARDUINO_DIRECTORIES_DATA/packages"
              for index in arduino-cli.yaml library_index.json package_index.json package_esp32_index.json; do
                ln -sfn "$ARDUINO_NIX_DATA/$index" "$ARDUINO_DIRECTORIES_DATA/$index"
              done
              echo "PRTSN development shell"
              echo "Arduino ESP32 Core 3.3.8 is provided by Nix."
              echo "Run 'make check' to verify the board setup."
            '';
          };
        });
    };
}
