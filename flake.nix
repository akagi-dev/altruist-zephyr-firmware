{
  description = "A very basic Zephyr flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # Customize the version of Zephyr used by the flake here
    zephyr.url = "github:zephyrproject-rtos/zephyr/v3.7.0";
    zephyr.flake = false;

    zephyr-nix.url = "github:nix-community/zephyr-nix";
    zephyr-nix.inputs.nixpkgs.follows = "nixpkgs";
    zephyr-nix.inputs.zephyr.follows = "zephyr";
  };

  outputs = { self, nixpkgs, zephyr-nix, ... }: let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;
    zephyr = zephyr-nix.packages.x86_64-linux;
  in {
    devShells.x86_64-linux.default = with pkgs; mkShell {
      packages = [
        (zephyr.sdk.override {targets = ["xtensa-espressif_esp32s3_zephyr-elf"];})
        zephyr.pythonEnv
        # Use zephyr.hosttools-nix to use nixpkgs built tooling instead of official Zephyr binaries
        zephyr.hosttools
        ninja
        cmake
      ];
    };
  };
}
