#include <iostream>
#include <jni.h>
#include <rfb/rfb.h>
#include <pthread.h>
#include <cstdlib>
#include <chrono>

#define FPS_DEFAULT 25
#define FPS_LIMIT 7
#define FPS_TIME_LIMIT 5000
#define FPS_NON -1
#define FPS_EVENT_NON -999

struct EKey {
    int key;
    bool down;
};

struct EMouse {
    int x;
    int y;
    int mask;
};

pthread_t thread_id;
rfbScreenInfoPtr server;

int maxx = 1;
int maxy = 1;
int bpp = 4;
char* tmpfb;
jobject globalKeyboard, globalMouse, globalDisplay;

EKey* eventKeyList = nullptr;
EMouse* eventMouseList = nullptr;
int eventFps = FPS_EVENT_NON;
int eventFpsLasted = FPS_NON;
long eventFpsTimeLimit = 0;
int sizeKey = 0;
int sizeMouse = 0;

long long currentFpsTime = 0;
int currentFps = 0;

static long long getCurrentTimeMs() {
    auto currentTime = std::chrono::system_clock::now();
    auto duration = currentTime.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return millis;
}

static void countFps() {
    long long t = getCurrentTimeMs();
    currentFps++;
    if (t - currentFpsTime > 1000) {
        std::cout << "fps: " << currentFps << std::endl;
        currentFps = 0;
        currentFpsTime = getCurrentTimeMs(); 
    }
}

static void dirtyCopy(const char* data, int width, int height, int nbytes) {
    const char* framebuffer = server->frameBuffer;
    char* modifiedFramebuffer = server->frameBuffer;
    
    for (int y = 0; y < height; y++) {
        const void* row1 = &framebuffer[y * width * nbytes];
        const void* row2 = &data[y * width * nbytes];
        if (memcmp(row1, row2, width * nbytes) != 0) {
            memcpy(&modifiedFramebuffer[y * width * nbytes], &data[y * width * nbytes], width * nbytes);
            rfbMarkRectAsModified(server, 0, y, width, y + 1);
        }
    }
}


static void callEvent(JNIEnv* env) {
    bool hasEvent = (sizeKey > 0 || sizeMouse > 0);
    long t = getCurrentTimeMs();
    if (hasEvent) {
        eventFpsTimeLimit = t;
    }
    if (hasEvent && eventFpsLasted != FPS_DEFAULT) {
        eventFps = FPS_DEFAULT;
        std::cout << "auto default fps " << eventFps << std::endl;
    }
    if (!hasEvent && t - eventFpsTimeLimit > FPS_TIME_LIMIT && eventFpsLasted != FPS_LIMIT) {
        eventFps = FPS_LIMIT;
        std::cout << "auto limit fps " << eventFps << std::endl;
    }

    // event key
    jclass callbackClassKeyboard = env->GetObjectClass(globalKeyboard);
    jmethodID callbackMethodKeyboard = env->GetMethodID(callbackClassKeyboard, "hookKeyPress", "(IZ)V");
    for(int i = 0; i < sizeKey; i++) {
        env->CallVoidMethod(globalKeyboard, callbackMethodKeyboard, eventKeyList[i].key, eventKeyList[i].down);
    }
    sizeKey = 0;

    // event mouse
    jclass callbackClassMouse = env->GetObjectClass(globalMouse);
    jmethodID callbackMethodMouse = env->GetMethodID(callbackClassMouse, "hookMouse", "(III)V");
    for(int i = 0; i < sizeMouse; i++) {
        env->CallVoidMethod(globalMouse, callbackMethodMouse, eventMouseList[i].x, eventMouseList[i].y, eventMouseList[i].mask);
    }
    sizeMouse = 0;

    // event fps
    if (eventFps != -999) {
        std::cout << "set fps " << eventFps << std::endl;
        jclass callbackClassFps = env->GetObjectClass(globalDisplay);
        jmethodID callbackMethodFps = env->GetMethodID(callbackClassFps, "setFps", "(I)V");
        env->CallVoidMethod(globalDisplay, callbackMethodFps, eventFps);
        eventFpsLasted = eventFps;
        eventFps = -999;
    }
}

static enum rfbNewClientAction newClient(rfbClientPtr cl) {
}

static void keyCallback(rfbBool down, rfbKeySym keySym, rfbClientPtr client)
{
    (void)(client);
    // std::cout << keySym << " - " << +down << std::endl;
    if (keySym > 65000) {
        keySym -= 65324;
    } else if (keySym >= 97 && keySym <= 122) {
        keySym -= 32;
    }
    eventKeyList[sizeKey].key = keySym; 
    eventKeyList[sizeKey].down = down; 
    sizeKey++;
}

static void mouseCallback(int buttonMask, int x, int y, rfbClientPtr client)
{
    (void)(client);
    // std::cout << +buttonMask << " - " << x << " - " << y << std::endl;
    eventMouseList[sizeMouse].x = x; 
    eventMouseList[sizeMouse].y = y; 
    eventMouseList[sizeMouse].mask = buttonMask; 
    sizeMouse++;
}

static jboolean hasConnection() {
    rfbClientIteratorPtr iter = rfbGetClientIterator(server);
    while (rfbClientPtr client = rfbClientIteratorNext(iter)) {
        rfbReleaseClientIterator(iter);
        return true;
    }
    rfbReleaseClientIterator(iter);
    return false;
}

extern "C" JNIEXPORT jboolean JNICALL Java_org_microemu_device_j2se_J2SEDeviceDisplay_updateProcess(JNIEnv* env, jobject obj) {
    rfbProcessEvents(server, 0);
    return hasConnection();
}

extern "C" JNIEXPORT jboolean JNICALL Java_org_microemu_device_j2se_J2SEDeviceDisplay_drawPixels(JNIEnv* env, jobject obj, jbyteArray pixels, jint width, jint height) {
    if (width != maxx || height != maxy) {
        char* oldfb = server->frameBuffer;
        maxx = width;
        maxy = height;
        char* newfb = (char*)malloc(maxx * maxy * bpp);
        rfbNewFramebuffer(server, (char*)newfb, maxx, maxy, 8, 3, bpp);
        free(oldfb);
        std::cout << "Change: " << maxx << ":" << maxy  << std::endl;
    }

    countFps();

    jbyte* pixelData = env->GetByteArrayElements(pixels, NULL);
    char* byteData = (char*)pixelData;
    dirtyCopy(byteData, width,  height, sizeof(int));
    while (rfbProcessEvents(server, 0)) {
    }
    callEvent(env);

    env->ReleaseByteArrayElements(pixels, pixelData, 0);
    return hasConnection();
}


static void *thr_handle(void *args) 
{
    pthread_t tid = pthread_self();
    int argc;
    char *agv;
    server = rfbGetScreen(&argc, &agv, maxx, maxy, 8, 3, bpp);
    tmpfb = (char*)malloc(maxx * maxy * bpp);
    server->frameBuffer= (char*)malloc(maxx*maxy*bpp);
    server->alwaysShared = TRUE;
    server->newClientHook = newClient;
    server->ptrAddEvent = mouseCallback;
    server->kbdAddEvent = keyCallback;
    rfbInitServer(server);
}

extern "C" JNIEXPORT void JNICALL Java_org_microemu_app_Main_initNative(JNIEnv *env, jobject) {
    std::cout << "Hello J2RUN!" << std::endl;
    eventKeyList = new EKey[100];
    eventMouseList = new EMouse[1000];
    int ret;
    if (ret = pthread_create(&thread_id, NULL, &thr_handle, NULL)) {
        return;
    }
}

extern "C" JNIEXPORT void JNICALL Java_org_microemu_app_ui_swing_SwingDeviceComponent_setObject(JNIEnv *env, jobject obj) {
    std::cout << "setObject keyboard!" << std::endl;
    globalKeyboard = env->NewGlobalRef(obj);
}

extern "C" JNIEXPORT void JNICALL Java_org_microemu_app_ui_swing_SwingDisplayComponent_setObject(JNIEnv *env, jobject obj) {
    std::cout << "setObject mouse!" << std::endl;
    globalMouse = env->NewGlobalRef(obj);
}


extern "C" JNIEXPORT jboolean JNICALL Java_org_microemu_device_j2se_J2SEDeviceDisplay_setObject(JNIEnv* env, jobject obj) {
    std::cout << "setObject display!" << std::endl;
    globalDisplay = env->NewGlobalRef(obj);
}

// extern "C" JNIEXPORT jboolean JNICALL Java_org_microemu_device_j2se_J2SEDeviceDisplay_drawIntPixels(JNIEnv* env, jobject obj, jintArray pixels, jint width, jint height) {
//     int sizeArr = width * height * bpp;
//     if (width != maxx || height != maxy) {
//         char* oldfb = server->frameBuffer;
//         maxx = width;
//         maxy = height;
//         free(tmpfb);
//         tmpfb = (char*)malloc(sizeArr);
//         char* newfb = (char*)malloc(sizeArr);
//         rfbNewFramebuffer(server, (char*)newfb, maxx, maxy, 8, 3, bpp);
//         free(oldfb);
//         std::cout << "Change: " << maxx << ":" << maxy  << std::endl;
//     }
    
//     jint* pixelIntData = env->GetIntArrayElements(pixels, NULL);
//     jsize numElements = env->GetArrayLength(pixels);
    
//     int pixel;
//     for (int i = 0; i < numElements; i++) {
//         pixel = pixelIntData[i];
//         tmpfb[i * 4 + 0] = (pixel >> 16) & 0xFF;
//         tmpfb[i * 4 + 1] = (pixel >> 8) & 0xFF;
//         tmpfb[i * 4 + 2] = pixel & 0xFF;
//         tmpfb[i * 4 + 3] = (pixel >> 24) & 0xFF;
//     }

//     dirtyCopy(tmpfb, width,  height, bpp);
//     rfbProcessEvents(server, 0);
//     env->ReleaseIntArrayElements(pixels, pixelIntData, 0);
//     return hasConnection();
// }