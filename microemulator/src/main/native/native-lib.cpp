#include <iostream>
#include "org_microemu_app_Main.h"
#include <rfb/rfb.h>
#include <pthread.h>
#include <cstdlib>

pthread_t thread_id;
rfbScreenInfoPtr server;

int maxx = 400;
int maxy = 542;
int bpp = 4;

static void *thr_handle(void *args) 
{
    pthread_t tid = pthread_self();
    int argc;
    char *a;
    server = rfbGetScreen(&argc, &a, maxx, maxy,8,3,4);
    server->frameBuffer= (char*)malloc(maxx*maxy*4);
    server->alwaysShared = TRUE;
    rfbInitServer(server);
}


extern "C" JNIEXPORT void JNICALL Java_org_microemu_device_j2se_J2SEDeviceDisplay_drawPixels(JNIEnv* env, jobject obj, jbyteArray pixels, jint width, jint height) {
    jbyte* pixelData = env->GetByteArrayElements(pixels, NULL);
    char* byteData = (char*)pixelData;
    memcpy(server->frameBuffer, byteData, width * height * sizeof(int));
    rfbMarkRectAsModified(server, 0, 0, width, height);
    rfbProcessEvents(server, 0);
    env->ReleaseByteArrayElements(pixels, pixelData, 0);
}


extern "C" JNIEXPORT void JNICALL Java_org_microemu_app_Main_drawPixels(JNIEnv* env, jobject obj, jbyteArray pixels, jint width, jint height) {
    Java_org_microemu_device_j2se_J2SEDeviceDisplay_drawPixels(env, obj, pixels, width, height);
}


extern "C" JNIEXPORT void JNICALL Java_org_microemu_app_Headless_printHello(JNIEnv *env, jobject obj) {
    Java_org_microemu_app_Main_printHello(env, obj);
}

extern "C" JNIEXPORT void JNICALL Java_org_microemu_app_Main_printHello(JNIEnv *, jobject) {
    std::cout << "Hello from C++!" << std::endl;

    int ret;
    if (ret = pthread_create(&thread_id, NULL, &thr_handle, NULL)) {
        return;
    }
}