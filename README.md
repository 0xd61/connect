# connect
Connect App für "zu Hause"

Mit der Zu Hause - Connect haben Sie den aktuellen Songtext während einer "Zu Hause" Veranstaltung direkt auf Ihrem Gerät. Sie haben die Möglichkeit die Textgröße und den Hintergrund auf Ihre Bedürfnisse anzupassen.

Made by Daniel Glinka from Samuu Tech S.R.L.
https://github.com/0xd61/connect/#readme

## Android Setup

Download the sdk commandline tools. Extract the downloaded file and rename the commandline tools folder to "tools" and move it under
`ANDROID_SDK_ROOT/cmdline-tools`.
The structure should be `ANDROID_SDK_ROOT/cmdline-tools/tools/bin/sdkmanager`.

Export the following env variables:
```
ANDROID_HOME="$HOME/Sources/android-sdk"
ANDROID_SDK_ROOT="$ANDROID_HOME"
# NOTE: somehow the build only worked when we put in the version explicitly
ANDROID_NDK_HOME="$ANDROID_HOME/ndk/21.1.6352462/"
PATH=$PATH:$ANDROID_SDK_ROOT/cmdline-tools/tools/bin
PATH=$PATH:$ANDROID_SDK_ROOT/emulator/

export ANDROID_HOME
export ANDROID_SDK_ROOT
export ANDROID_NDK_HOME
```

Download the following ndk package:
```
sdkmanager "ndk;21.1.6352462"
```

Then run the android sdl build
```
./build.sh android
```

## Installed Packages on working build

```
Path                                    | Version      | Description                     | Location
-------                                 | -------      | -------                         | -------
build-tools;26.0.3                      | 26.0.3       | Android SDK Build-Tools 26.0.3  | build-tools/26.0.3/
emulator                                | 30.2.6       | Android Emulator                | emulator/

ndk;21.1.6352462                        | 21.1.6352462 | NDK (Side by side) 21.1.6352462 | ndk/21.1.6352462/
patcher;v4                              | 1            | SDK Patch Applier v4            | patcher/v4/

platform-tools                          | 30.0.5       | Android SDK Platform-Tools      | platform-tools/
platforms;android-26                    | 2            | Android SDK Platform 26         | platforms/android-26/
system-images;android-26;default;x86_64 | 1            | Intel x86 Atom_64 System Image  | system-images/android-26/default/x86_64/
  ```

  ## Android Debugging

  The logmessages on android are tagged with co.degit.connect. You can view them via `adb logcat` with the following command:

  ```
  adb logcat -v color SDL:V co.degit.connect:V *:E
  ```
