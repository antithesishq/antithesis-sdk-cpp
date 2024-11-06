{stdenv, pkgs}:
  with pkgs;
  stdenv.mkDerivation {
    pname = "antithesis-sdk-cpp";
    version = "0.4.3";

    src = ./.;

    installPhase = ''
      mkdir -p $out/include
      cp antithesis_sdk.h $out/include/antithesis_sdk.h
      cp antithesis_instrumentation.h $out/include/antithesis_instrumentation.h
    '';

  }
