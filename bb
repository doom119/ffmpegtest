#!/bin/bash
ndk-build NDK_DEBUG=1 &&
gradle build &&
adb install -r build/outputs/apk/FFmpegTest-debug.apk &&
adb shell am start -n com.doom119.ffmpegtest/.MainActivity
