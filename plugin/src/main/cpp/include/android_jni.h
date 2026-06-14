#pragma once

#ifdef __ANDROID__
#include <jni.h>

// Returns the JavaVM captured during JNI_OnLoad.
// Valid for the lifetime of the process once the library is loaded.
JavaVM *get_android_jvm();
#endif
