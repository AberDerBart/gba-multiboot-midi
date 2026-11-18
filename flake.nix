{
  inputs = {
  };

  outputs = { self, nixpkgs }:
    let
      pkgs = import nixpkgs {
        system = "x86_64-linux";
      };
      pico-sdk-full = pkgs.pico-sdk.override {
        withSubmodules = true;
      };
    in
    {
      packages.x86_64-linux.elf2uf2-rs = pkgs.callPackage ./elf2uf2.nix { };
      
      packages.x86_64-linux.gba-config = pkgs.writers.writePython3Bin "gba-synth-config" {
        libraries = [ pkgs.python3Packages.pyserial ];
      } (builtins.readFile ./tools/config.py);

      devShell.x86_64-linux = pkgs.mkShell {
        nativeBuildInputs = [
          pkgs.openocd-rp2040
        ];
        buildInputs = [
          pkgs.cmake
          pkgs.git
          pkgs.gcc-arm-embedded
          pico-sdk-full
          
          pkgs.rtmidi
          pkgs.alsa-utils
          pkgs.minicom

          pkgs.yarn

          pkgs.python3
          pkgs.python3Packages.pyserial
          pkgs.python3Packages.autopep8
        ];

        PICO_SDK_PATH = "${pico-sdk-full}/lib/pico-sdk";
      };
    };
}
