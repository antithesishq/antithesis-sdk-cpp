# antithesis-sdk-cpp.nix

{stdenv, pkgs}:
# let
# in
  with pkgs;
  stdenv.mkDerivation {
    pname = "antithesis-sdk-cpp";
    version = "0.1.1";

    src = ./.;

    nativeBuildInputs = [
      clang_17
      cmake
    ];

    # buildInputs = [];

    configurePhase = ''
      cmake .
    '';

    # patchPhase = '' '';

    buildPhase = ''
      #env
      make antithesis-sdk-cpp
      #ls -lR 
    '';

    # checkPhase = '' '';

    installPhase = ''
      mkdir -p $out/include
      cp antithesis_sdk.h $out/include/antithesis_sdk.h
    '';

  }
