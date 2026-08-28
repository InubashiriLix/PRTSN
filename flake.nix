{
  description = "PRTSN firmware development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          python = pkgs.python3.withPackages (pythonPackages: [
            pythonPackages.pillow
          ]);
          esp32ArduinoCore = pkgs.callPackage ./nix/esp32-arduino-core.nix { };
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

            ARDUINO_DIRECTORIES_DATA = "${esp32ArduinoCore}/share/arduino";
            ARDUINO_ESP32_TOOLS = "${esp32ArduinoCore}/share/arduino/packages/esp32/tools";

            shellHook = ''
              export ARDUINO_DIRECTORIES_DOWNLOADS="$PWD/.cache/arduino-cli/downloads"
              export ARDUINO_DIRECTORIES_USER="$PWD/.cache/arduino-cli/user"
              mkdir -p "$ARDUINO_DIRECTORIES_DOWNLOADS" "$ARDUINO_DIRECTORIES_USER"
              echo "PRTSN development shell"
              echo "Arduino ESP32 Core 3.3.8 is provided by Nix."
              echo "Run 'make check' to verify the board setup."
            '';
          };
        });
    };
}
