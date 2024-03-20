# default.nix

let pkgs = import <nixpkgs> { }; in
{
  antithesis-sdk-cpp = pkgs.callPackage ./antithesis-sdk-cpp.nix  { stdenv = pkgs.clangStdenv; };
} 
