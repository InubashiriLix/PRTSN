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

            shellHook = ''
              echo "PRTSN development shell"
              echo "Run 'make check' to verify the ESP32 core and board setup."
            '';
          };
        });
    };
}
