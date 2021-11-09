# This shell defines a development environment for a c project.
{
  pkgs ? import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/39e6bf76474ce742eb027a88c4da6331f0a1526f.tar.gz") {
    config.android_sdk.accept_license = true;
    config.allowUnfree = true;
  }
}:


let
  buildToolsVersion = "30.0.3";

  androidComposition = pkgs.androidenv.composeAndroidPackages {
    includeEmulator = true;
    platformVersions = [ "26" "29" "30" ];
    includeSources = false;
    includeSystemImages = false;
    systemImageTypes = [ "google_apis_playstore" ];
    abiVersions = [ "armeabi-v7a" "arm64-v8a" ];
    includeNDK = true;
    ndkVersions = ["22.0.7026061"];
    useGoogleAPIs = false;
    useGoogleTVAddOns = false;
    includeExtras = [
      "extras;google;gcm"
    ];
  };
in
pkgs.mkShell.override {stdenv = pkgs.llvmPackages_10.stdenv;} {
   buildInputs = with pkgs; [
       android-studio
       androidComposition.androidsdk
       pkg-config
       SDL2
       SDL2_net
   ];

   hardeningDisable = [ "fortify" ];

   shellHook = ''

      export ANDROID_HOME="${androidComposition.androidsdk}/libexec/android-sdk"
      export ANDROID_SDK_ROOT="${androidComposition.androidsdk}/libexec/android-sdk";
      export ANDROID_HOME="${androidComposition.androidsdk}/libexec/android-sdk";
      export ANDROID_NDK_HOME="${androidComposition.androidsdk}/libexec/android-sdk/ndk-bundle";

      # start user default shell
      #SHELL=$(awk -F: -v user="$USER" '$1 == user {print $NF}' /etc/passwd)
      #$SHELL
  '';

   NIX_ENFORCE_PURITY=0;
}
