#include <iostream>
#include <jni.h>
#include <rfb/rfb.h>
#include <pthread.h>
#include <thread>
#include <cstdlib>
#include <chrono>

#define FPS_DEFAULT 25
#define FPS_LIMIT 10
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
jobject globalKeyboard, globalMouse, globalJ2seVnc;

EKey* eventKeyList = nullptr;
EMouse* eventMouseList = nullptr;
int eventFps = FPS_EVENT_NON;
int eventFpsLasted = FPS_NON;
long long eventFpsTimeLimit = 0;
int sizeKey = 0;
int sizeMouse = 0;

long long currentFpsTime = 0;
int currentFps = 0;

JavaVM *globalJvm;
bool computeSize = true;
int countConnection = 0;

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
    int bkSizeKey = sizeKey;
    int bkSizeMouse = sizeMouse;
    sizeKey = 0;
    sizeMouse = 0;
    bool hasEvent = (bkSizeKey > 0 || bkSizeMouse > 0);
    long long currentTime = getCurrentTimeMs();
    if (hasEvent) {
        eventFpsTimeLimit = currentTime;
        // std::cout << "event: mouse(" << bkSizeMouse << ") key (" << bkSizeKey << ")" << std::endl;
    }
    if (hasEvent && eventFpsLasted != FPS_DEFAULT) {
        eventFps = FPS_DEFAULT;
        std::cout << "auto default fps " << eventFps << std::endl;
    }
    if (!hasEvent && currentTime - eventFpsTimeLimit > FPS_TIME_LIMIT && eventFpsLasted != FPS_LIMIT) {
        eventFps = FPS_LIMIT;
        std::cout << "auto limit fps " << eventFps << std::endl;
    }

    // event key
    if (bkSizeKey > 0) {
        jclass callbackClassKeyboard = env->GetObjectClass(globalKeyboard);
        jmethodID callbackMethodKeyboard = env->GetMethodID(callbackClassKeyboard, "hookKeyPress", "(IZ)V");
        for(int i = 0; i < bkSizeKey; i++) {
            env->CallVoidMethod(globalKeyboard, callbackMethodKeyboard, eventKeyList[i].key, eventKeyList[i].down);
        }
    }


    // event mouse
    if (bkSizeMouse > 0) {
        jclass callbackClassMouse = env->GetObjectClass(globalMouse);
        jmethodID callbackMethodMouse = env->GetMethodID(callbackClassMouse, "hookMouse", "(III)V");
        for(int i = 0; i < bkSizeMouse; i++) {
            env->CallVoidMethod(globalMouse, callbackMethodMouse, eventMouseList[i].x, eventMouseList[i].y, eventMouseList[i].mask);
        }
    }

    // event fps
    if (eventFps != -999) {
        std::cout << "set fps " << eventFps << std::endl;
        jclass callbackClassFps = env->GetObjectClass(globalJ2seVnc);
        jmethodID callbackMethodFps = env->GetMethodID(callbackClassFps, "setFps", "(I)V");
        env->CallVoidMethod(globalJ2seVnc, callbackMethodFps, eventFps);
        eventFpsLasted = eventFps;
        eventFps = -999;
    }
}


static void clientgone(rfbClientPtr cl)
{
    countConnection--;
}

static enum rfbNewClientAction newClient(rfbClientPtr cl) {
    cl->clientGoneHook = clientgone;
    countConnection++;
    return RFB_CLIENT_ACCEPT;
}

static void keyCallback(rfbBool down, rfbKeySym keySym, rfbClientPtr client)
{
    (void)(client);
    // std::cout << keySym << " - " << +down << std::endl;
    if (keySym >= 65456 && keySym <= 65465) {
        // 0 - 96 - 65456 -> 9 - 105 - 65465
        keySym -= 65360;
    } else if (keySym >= 65361 && keySym <= 65364) {
        // char
        keySym -= 65324;
    } else if (keySym == 65293) {
        // enter
        keySym = 10;
    } else if (keySym == 65288) {
        // backspace - 8 - 65288
        keySym = 8;
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

extern "C" JNIEXPORT jboolean JNICALL Java_org_microemu_device_j2se_J2SEVnc_updateProcess(JNIEnv* env, jobject obj) {
    countFps();
    rfbProcessEvents(server, 0);
    return countConnection != 0;
}

extern "C" JNIEXPORT jboolean JNICALL Java_org_microemu_device_j2se_J2SEVnc_drawPixels(JNIEnv* env, jobject obj, jbyteArray pixels, jint width, jint height, jint loopEvent) {
    if (computeSize) {
        computeSize = false;
        char* oldfb = server->frameBuffer;
        maxx = width;
        maxy = height;
        char* newfb = (char*)malloc(maxx * maxy * bpp);
        rfbNewFramebuffer(server, (char*)newfb, maxx, maxy, 8, 3, bpp);
        free(oldfb);
        std::cout << "Change: " << maxx << ":" << maxy  << std::endl;
    }
    
    // countFps();

    jbyte* pixelData = env->GetByteArrayElements(pixels, NULL);
    char* byteData = (char*)pixelData;
    dirtyCopy(byteData, maxx,  maxy, sizeof(int));
    while (rfbProcessEvents(server, 0) || loopEvent-- > 0) {
    }
    callEvent(env);

    env->ReleaseByteArrayElements(pixels, pixelData, 0);
    return countConnection != 0;
}


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* jvm, void* reserved) {
    globalJvm = jvm;
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* jvm, void* reserved) {
    globalJvm = nullptr; 
}

static void *thr_handle(void *args) 
{
    pthread_t tid = pthread_self();
    int argc;
    char *agv;
    server = rfbGetScreen(&argc, &agv, maxx, maxy, 8, 3, bpp);
    server->frameBuffer = (char*)malloc(maxx * maxy * bpp);
    server->alwaysShared = TRUE;
    server->newClientHook = newClient;
    server->ptrAddEvent = mouseCallback;
    server->kbdAddEvent = keyCallback;

    JNIEnv* env;
    jint result = globalJvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
    if (result != JNI_OK) {
        std::cerr << "Failed to attach thread to JVM" << std::endl;
        free(server->frameBuffer);
        return nullptr;
    }

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

extern "C" JNIEXPORT jboolean JNICALL Java_org_microemu_device_j2se_J2SEVnc_setObject(JNIEnv* env, jobject obj) {
    std::cout << "setObject j2se vnc!" << std::endl;
    globalJ2seVnc = env->NewGlobalRef(obj);
}