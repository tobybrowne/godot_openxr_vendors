#ifdef __ANDROID__
#include <jni.h>
#include "android_jni.h"

static JavaVM *s_jvm = nullptr;

JavaVM *get_android_jvm() {
	return s_jvm;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
	s_jvm = vm;
	return JNI_VERSION_1_6;
}
#endif
