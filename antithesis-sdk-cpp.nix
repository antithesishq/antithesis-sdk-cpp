{stdenv, pkgs}:
  with pkgs;
  stdenv.mkDerivation {
    pname = "antithesis-sdk-cpp";
    version = "0.2.3";

    src = ./.;

    installPhase = ''
      mkdir -p $out/include
      cp antithesis_sdk.h $out/include/antithesis_sdk.h
    '';

  }
