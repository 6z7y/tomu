# {
#   description = "is just a Music player";
#   inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
#
#   outputs =
#     { self, nixpkgs }:
#     let
#       systems = [
#         "x86_64-linux"
#         "aarch64-linux"
#       ];
#       forAllSystems = nixpkgs.lib.genAttrs systems;
#       pkgsFor = system: nixpkgs.legacyPackages.${system};
#     in
#     {
#       packages = forAllSystems (system: {
#         default = (pkgsFor system).callPackage ./default.nix { };
#       });
#
#       devShells = forAllSystems (
#         system:
#         let
#           pkgs = pkgsFor system;
#         in
#         {
#           default = pkgs.mkShell {
#             inputsFrom = [ self.packages.${system}.default ];
#
#             packages = with pkgs; [
#               gdb # Debugger
#               valgrind # Memory leak checker
#               clang-tools # For auto-formatting C code
#             ];
#           };
#         }
#       );
#
#       formatter = forAllSystems (system: (pkgsFor system).nixfmt-rfc-style);
#     };
# }
# In your tomu project folder:

{
  description = "Tomu";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }: {
    packages.x86_64-linux.default = nixpkgs.legacyPackages.x86_64-linux.callPackage ./default.nix {};
  };
}
