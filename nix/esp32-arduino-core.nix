{
  lib,
  stdenvNoCC,
  arduino-cli,
  fetchurl,
}:

let
  version = "3.3.8";
  host = "x86_64-pc-linux-gnu";

  mkArchive =
    {
      url,
      archiveFileName,
      sha256,
      size,
    }:
    {
      inherit
        url
        archiveFileName
        sha256
        size
        ;
      source = fetchurl {
        name = archiveFileName;
        inherit url sha256;
      };
    };

  mkTool =
    name: toolVersion: archive:
    {
      inherit name;
      version = toolVersion;
      systems = [
        {
          inherit host;
          inherit (archive)
            url
            archiveFileName
            size
            ;
          checksum = "SHA-256:${archive.sha256}";
        }
      ];
    };

  core = mkArchive {
    url = "https://github.com/espressif/arduino-esp32/releases/download/${version}/esp32-core-${version}.zip";
    archiveFileName = "esp32-core-${version}.zip";
    sha256 = "8159cefaa2eea131360ac32393d741d462583297c8c1e5169ca48139fa324173";
    size = "27752624";
  };

  espX32 = mkArchive {
    url = "https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20260121/xtensa-esp-elf-14.2.0_20260121-x86_64-linux-gnu.tar.gz";
    archiveFileName = "xtensa-esp-elf-14.2.0_20260121-x86_64-linux-gnu.tar.gz";
    sha256 = "4e090a6cbf1ff7769684d9a248c9b8bdbe4c0ada098adda54b4c67a91449afa7";
    size = "334265784";
  };

  xtensaGdb = mkArchive {
    url = "https://github.com/espressif/binutils-gdb/releases/download/esp-gdb-v16.3_20250913/xtensa-esp-elf-gdb-16.3_20250913-x86_64-linux-gnu.tar.gz";
    archiveFileName = "xtensa-esp-elf-gdb-16.3_20250913-x86_64-linux-gnu.tar.gz";
    sha256 = "16d05c9104ff84529ac3799abb04d5666c193131ab461f153040721728b48730";
    size = "36396804";
  };

  espRv32 = mkArchive {
    url = "https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20260121/riscv32-esp-elf-14.2.0_20260121-x86_64-linux-gnu.tar.gz";
    archiveFileName = "riscv32-esp-elf-14.2.0_20260121-x86_64-linux-gnu.tar.gz";
    sha256 = "1b79617f43cf0e25b92646359d1623d8bef135050aca44586df59b6a64496d82";
    size = "590607738";
  };

  riscvGdb = mkArchive {
    url = "https://github.com/espressif/binutils-gdb/releases/download/esp-gdb-v16.3_20250913/riscv32-esp-elf-gdb-16.3_20250913-x86_64-linux-gnu.tar.gz";
    archiveFileName = "riscv32-esp-elf-gdb-16.3_20250913-x86_64-linux-gnu.tar.gz";
    sha256 = "4e3cf8b7d11c7a2d1b50f40b1c50c0671dfe7eb13782c27c8a8cfdc8548bcdd4";
    size = "36557187";
  };

  openocd = mkArchive {
    url = "https://github.com/espressif/openocd-esp32/releases/download/v0.12.0-esp32-20251215/openocd-esp32-linux-amd64-0.12.0-esp32-20251215.tar.gz";
    archiveFileName = "openocd-esp32-linux-amd64-0.12.0-esp32-20251215.tar.gz";
    sha256 = "5e6ff40aeca23bdd203cde04d60bc808c0e6bff110eadcbce3d602618c880531";
    size = "2547606";
  };

  esptool = mkArchive {
    url = "https://github.com/espressif/esptool/releases/download/v5.2.0/esptool-v5.2.0-linux-amd64.tar.gz";
    archiveFileName = "esptool-v5.2.0-linux-amd64.tar.gz";
    sha256 = "0a9f6c913fccfacac9261eb2acd8060010db5933c18c8e47cb32377eaa7202a3";
    size = "73991509";
  };

  mkspiffs = mkArchive {
    url = "https://github.com/igrr/mkspiffs/releases/download/0.2.3/mkspiffs-0.2.3-arduino-esp32-linux64.tar.gz";
    archiveFileName = "mkspiffs-0.2.3-arduino-esp32-linux64.tar.gz";
    sha256 = "5e1a4ff41385e842f389f6b5254102a547e566a06b49babeffa93ef37115cb5d";
    size = "50646";
  };

  mklittlefs = mkArchive {
    url = "https://github.com/earlephilhower/mklittlefs/releases/download/4.0.2/x86_64-linux-gnu-mklittlefs-db0513a.tar.gz";
    archiveFileName = "x86_64-linux-gnu-mklittlefs-db0513a.tar.gz";
    sha256 = "7a70428b7089bf1c9d481b0e070e99cd8a430d37e197b7d3db64f24ba8891508";
    size = "54000";
  };

  dfuUtil = mkArchive {
    url = "https://downloads.arduino.cc/tools/dfu-util-0.11-arduino5-linux_amd64.tar.gz";
    archiveFileName = "dfu-util-0.11-arduino5-linux_amd64.tar.gz";
    sha256 = "96c64c278561af806b585c123c85748926ad02b1aedc07e5578ca9bee2be0d2a";
    size = "2283425";
  };

  esp32c3Libs = mkArchive {
    url = "https://github.com/espressif/arduino-esp32/releases/download/${version}/esp32c3-libs-${version}.zip";
    archiveFileName = "esp32c3-libs-${version}.zip";
    sha256 = "9157ef480198ff387444649e5a9362840b3fc4f35e481697fb68cb9f2a9fb04a";
    size = "55238122";
  };

  esp32s3Libs = mkArchive {
    url = "https://github.com/espressif/arduino-esp32/releases/download/${version}/esp32s3-libs-${version}.zip";
    archiveFileName = "esp32s3-libs-${version}.zip";
    sha256 = "fc9dcf416ec458527b932d3423c337eb207e90492367a390c1408ecde7cc505c";
    size = "65098729";
  };

  ctags = mkArchive {
    url = "https://downloads.arduino.cc/tools/ctags-5.8-arduino11-pm-x86_64-pc-linux-gnu.tar.bz2";
    archiveFileName = "ctags-5.8-arduino11-pm-x86_64-pc-linux-gnu.tar.bz2";
    sha256 = "62b514f3aaf37b5429ef703853bb46365fb91b4754c1916d085bf134004886e3";
    size = "111604";
  };

  dfuDiscovery = mkArchive {
    url = "https://downloads.arduino.cc/discovery/dfu-discovery/dfu-discovery_v0.1.2_Linux_64bit.tar.gz";
    archiveFileName = "dfu-discovery_v0.1.2_Linux_64bit.tar.gz";
    sha256 = "d82649741fd6cbe369d7fe22d290efd7e28e4d6fd0b81b7b1eabff43b495b5f8";
    size = "1731768";
  };

  mdnsDiscovery = mkArchive {
    url = "https://downloads.arduino.cc/discovery/mdns-discovery/mdns-discovery_v1.1.0_Linux_64bit.tar.gz";
    archiveFileName = "mdns-discovery_v1.1.0_Linux_64bit.tar.gz";
    sha256 = "6966f406e5b6e80d82a388706d6d9d8a63d46035d19d1e6b4fb1ebeaf7fea056";
    size = "3220729";
  };

  serialDiscovery = mkArchive {
    url = "https://downloads.arduino.cc/discovery/serial-discovery/serial-discovery_v1.5.2_Linux_64bit.tar.gz";
    archiveFileName = "serial-discovery_v1.5.2_Linux_64bit.tar.gz";
    sha256 = "7a95d7386c66c0846fe4aa96b601e72783ae64f541eb2f5edb4fc9e69a04b6d3";
    size = "2163207";
  };

  serialMonitor = mkArchive {
    url = "https://downloads.arduino.cc/monitor/serial-monitor/serial-monitor_v0.15.0_Linux_64bit.tar.gz";
    archiveFileName = "serial-monitor_v0.15.0_Linux_64bit.tar.gz";
    sha256 = "f1bc9680f2cff08688bc4ccbaf5e9be4ce38aa3f10d40a487a7704c04006f19b";
    size = "2446183";
  };

  esp32ToolDefinitions = [
    (mkTool "esp-x32" "2601" espX32)
    (mkTool "xtensa-esp-elf-gdb" "16.3_20250913" xtensaGdb)
    (mkTool "esp-rv32" "2601" espRv32)
    (mkTool "riscv32-esp-elf-gdb" "16.3_20250913" riscvGdb)
    (mkTool "openocd-esp32" "v0.12.0-esp32-20251215" openocd)
    (mkTool "esptool_py" "5.2.0" esptool)
    (mkTool "mkspiffs" "0.2.3" mkspiffs)
    (mkTool "mklittlefs" "4.0.2-db0513a" mklittlefs)
    (mkTool "esp32c3-libs" version esp32c3Libs)
    (mkTool "esp32s3-libs" version esp32s3Libs)
  ];

  builtinToolDefinitions = [
    (mkTool "ctags" "5.8-arduino11" ctags)
    (mkTool "dfu-discovery" "0.1.2" dfuDiscovery)
    (mkTool "mdns-discovery" "1.1.0" mdnsDiscovery)
    (mkTool "serial-discovery" "1.5.2" serialDiscovery)
    (mkTool "serial-monitor" "0.15.0" serialMonitor)
  ];

  toolDependency = packager: name: toolVersion: {
    inherit packager name;
    version = toolVersion;
  };

  esp32Index = builtins.toFile "package_esp32_index.json" (builtins.toJSON {
    packages = [
      {
        name = "esp32";
        maintainer = "Espressif Systems";
        websiteURL = "https://github.com/espressif/arduino-esp32";
        email = "";
        help.online = "";
        platforms = [
          {
            name = "esp32";
            architecture = "esp32";
            inherit version;
            category = "ESP32";
            inherit (core)
              url
              archiveFileName
              size
              ;
            checksum = "SHA-256:${core.sha256}";
            help.online = "";
            boards = [
              { name = "ESP32-C3 Dev Board"; }
              { name = "ESP32-S3 Dev Board"; }
            ];
            toolsDependencies = [
              (toolDependency "esp32" "esp-x32" "2601")
              (toolDependency "esp32" "xtensa-esp-elf-gdb" "16.3_20250913")
              (toolDependency "esp32" "esp-rv32" "2601")
              (toolDependency "esp32" "riscv32-esp-elf-gdb" "16.3_20250913")
              (toolDependency "esp32" "openocd-esp32" "v0.12.0-esp32-20251215")
              (toolDependency "esp32" "esptool_py" "5.2.0")
              (toolDependency "esp32" "mkspiffs" "0.2.3")
              (toolDependency "esp32" "mklittlefs" "4.0.2-db0513a")
              (toolDependency "arduino" "dfu-util" "0.11.0-arduino5")
              (toolDependency "esp32" "esp32c3-libs" version)
              (toolDependency "esp32" "esp32s3-libs" version)
            ];
          }
        ];
        tools = esp32ToolDefinitions;
      }
    ];
  });

  arduinoIndex = builtins.toFile "package_index.json" (builtins.toJSON {
    packages = [
      {
        name = "arduino";
        maintainer = "Arduino";
        websiteURL = "https://www.arduino.cc/";
        email = "";
        help.online = "";
        platforms = [ ];
        tools = [ (mkTool "dfu-util" "0.11.0-arduino5" dfuUtil) ];
      }
      {
        name = "builtin";
        maintainer = "Arduino";
        websiteURL = "https://www.arduino.cc/";
        email = "";
        help.online = "";
        platforms = [ ];
        tools = builtinToolDefinitions;
      }
    ];
  });

  libraryIndex = builtins.toFile "library_index.json" (builtins.toJSON {
    libraries = [ ];
  });

  archives = [
    core
    espX32
    xtensaGdb
    espRv32
    riscvGdb
    openocd
    esptool
    mkspiffs
    mklittlefs
    dfuUtil
    esp32c3Libs
    esp32s3Libs
    ctags
    dfuDiscovery
    mdnsDiscovery
    serialDiscovery
    serialMonitor
  ];

  stageArchives = lib.concatMapStringsSep "\n" (archive: ''
    ln -s ${archive.source} \
      "$ARDUINO_DIRECTORIES_DOWNLOADS/packages/${archive.archiveFileName}"
  '') archives;
in
stdenvNoCC.mkDerivation {
  pname = "arduino-esp32-core-data";
  inherit version;

  dontUnpack = true;
  nativeBuildInputs = [ arduino-cli.pureGoPkg ];

  buildPhase = ''
    runHook preBuild

    export HOME="$TMPDIR/home"
    export ARDUINO_DIRECTORIES_DATA="$out/share/arduino"
    export ARDUINO_DIRECTORIES_DOWNLOADS="$TMPDIR/downloads"
    export ARDUINO_DIRECTORIES_USER="$TMPDIR/user"

    mkdir -p \
      "$HOME" \
      "$ARDUINO_DIRECTORIES_DATA" \
      "$ARDUINO_DIRECTORIES_DOWNLOADS/packages" \
      "$ARDUINO_DIRECTORIES_USER"

    cp ${arduinoIndex} "$ARDUINO_DIRECTORIES_DATA/package_index.json"
    cp ${esp32Index} "$ARDUINO_DIRECTORIES_DATA/package_esp32_index.json"
    cp ${libraryIndex} "$ARDUINO_DIRECTORIES_DATA/library_index.json"

    ${stageArchives}

    arduino-cli config init
    arduino-cli config add \
      board_manager.additional_urls \
      https://espressif.github.io/arduino-esp32/package_esp32_index.json
    arduino-cli core install esp32:esp32@${version} --skip-post-install

    # Download caches and the inventory database are mutable Arduino CLI state.
    # The installed core and tools are the only data exported to the Nix store.
    rm -rf "$ARDUINO_DIRECTORIES_DATA/staging" "$ARDUINO_DIRECTORIES_DATA/tmp"
    rm -f "$ARDUINO_DIRECTORIES_DATA/inventory.yaml"
    mkdir -p "$ARDUINO_DIRECTORIES_DATA/staging"

    runHook postBuild
  '';

  installPhase = "true";

  meta = {
    description = "Pinned Arduino ESP32 core and C3/S3 toolchains for PRTSN";
    homepage = "https://github.com/espressif/arduino-esp32";
    license = lib.licenses.lgpl21Plus;
    platforms = [ "x86_64-linux" ];
  };
}
