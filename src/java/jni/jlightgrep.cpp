#include "lightgrep/api.h"
#include "jlightgrep.h"

#include <functional>
#include <limits>
#include <memory>
#include <tuple>

static const char* ALL_IS_LOST = "Fuck it, Dude. Let's go bowling.";

static const char* programHandleClassName = "com/lightboxtechnologies/lightgrep/ProgramHandle";
static const char* contextHandleClassName = "com/lightboxtechnologies/lightgrep/ContextHandle";

static const char* hitCallbackClassName = "com/lightboxtechnologies/lightgrep/HitCallback";
static const char* searchHitClassName = "com/lightboxtechnologies/lightgrep/SearchHit";

static const char* keywordExceptionClassName = "com/lightboxtechnologies/lightgrep/KeywordException";
static const char* programExceptionClassName = "com/lightboxtechnologies/lightgrep/ProgramException";

class PendingJavaException {};

void throwIfException(JNIEnv* env) {
  if (env->ExceptionCheck()) {
    throw PendingJavaException();
  }
}

static void throwException(JNIEnv* env, const char* exClassName, const char* message) {
  jclass ex = env->FindClass(exClassName);
  if (!env->ExceptionCheck()) {
    if (env->ThrowNew(ex, message)) {
      env->FatalError(ALL_IS_LOST);
    }
  }

  throw PendingJavaException();
}



//
// We cache field and method IDs in static init() methods for each class,
// in order to ensure that they get reset if a class is reloaded.
//

static jfieldID handlePointerField;

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_Handle_init(JNIEnv* env, jclass cl) {
  try {
    handlePointerField = env->GetFieldID(cl, "Pointer", "J");
    throwIfException(env);
  }
  catch (const PendingJavaException&) {
  }
}

static jmethodID programHandleCtor;

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_init(JNIEnv* env, jclass cl) {
  try {
    programHandleCtor = env->GetMethodID(cl, "<init>", "(J)V");
    throwIfException(env);
  }
  catch (const PendingJavaException&) {
  }
}

static jmethodID contextHandleCtor;

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_init(JNIEnv* env, jclass cl) {
  try {
    contextHandleCtor = env->GetMethodID(cl, "<init>", "(J)V");
    throwIfException(env);
  }
  catch (const PendingJavaException&) {
  }
}

static jfieldID keyOptionsFixedStringField;
static jfieldID keyOptionsCaseInsensitiveField;

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_KeyOptions_init(JNIEnv* env, jclass cl) {
  try {
    keyOptionsFixedStringField = env->GetFieldID(cl, "FixedString", "Z");
    throwIfException(env); 

    keyOptionsCaseInsensitiveField = env->GetFieldID(cl, "FixedString", "Z");
    throwIfException(env); 
  }
  catch (const PendingJavaException&) {
  }
}

static jfieldID programOptionsDeterminizeField;

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramOptions_init(JNIEnv* env, jclass cl) {
  try {
    programOptionsDeterminizeField = env->GetFieldID(cl, "Determinize", "Z");
    throwIfException(env); 
  }
  catch (const PendingJavaException&) {
  }
}

static jclass hitCallbackClass;
static jmethodID hitCallbackCallback;

static jclass searchHitClass;
static jmethodID searchHitCtor;
static jfieldID searchHitStartField;
static jfieldID searchHitEndField;
static jfieldID searchHitKeywordIndexField;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* jvm, void*) {
  try {
    JNIEnv* env;
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
      throw PendingJavaException();
    }

    // We make a global reference for HitCallback because it's not possible
    // to put a static block or any method implementations into an interface,
    // which prevents us from reseting the callback() method ID in the event
    // that HitCallback is unloaded and reloaded. The global reference blocks
    // this by preventing HitCallback from being unloaded.
    jclass cl = env->FindClass(hitCallbackClassName);
    throwIfException(env);

    hitCallbackClass = reinterpret_cast<jclass>(env->NewGlobalRef(cl));
    if (!hitCallbackClass) throw PendingJavaException();

    hitCallbackCallback = env->GetMethodID(hitCallbackClass, "callback", "(Lcom/lightboxtechnologies/lightgrep/SearchHit;)V");
    throwIfException(env);

    // We make a global reference for SearchHit to avoid calling FindClass
    // for it on every hit in the callbackShim.
    cl = env->FindClass(searchHitClassName);
    throwIfException(env);

    searchHitClass = reinterpret_cast<jclass>(env->NewGlobalRef(cl));
    throwIfException(env);

    searchHitCtor = env->GetMethodID(searchHitClass, "<init>", "(JJI)V");
    throwIfException(env);

    searchHitStartField = env->GetFieldID(searchHitClass, "Start", "J");
    throwIfException(env);

    searchHitEndField = env->GetFieldID(searchHitClass, "End", "J");
    throwIfException(env);

    searchHitKeywordIndexField = env->GetFieldID(searchHitClass, "KeywordIndex", "I");
    throwIfException(env);

    return JNI_VERSION_1_6;
  }
  catch (const PendingJavaException&) {
    return JNI_ERR;
  }
}

JNIEXPORT void JNI_OnUnload(JavaVM* jvm, void*) {
  JNIEnv* env;
  if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    // Everything is hosed if this happens. We can't even throw an
    // exception back into Java, since to do that we'd need a valid JNIEnv.
    return;
  }

  env->DeleteGlobalRef(hitCallbackClass);
  env->DeleteGlobalRef(searchHitClass);
}

/*
template <typename T> T unwrap(JNIEnv* env, jobject handle) { 
  return reinterpret_cast<T>(env->GetLongField(handle, handlePointerField));
}
*/

std::unique_ptr<const char,std::function<void(const char*)>> unwrap(JNIEnv* env, jstring str) {
  using namespace std::placeholders;

  std::unique_ptr<const char,std::function<void(const char*)>> ptr(
    env->GetStringUTFChars(str, nullptr),
    std::bind(&JNIEnv::ReleaseStringUTFChars, env, str, _1)
  );

  if (!ptr) {
    // OutOfMemoryError already thrown
    throw PendingJavaException();
  }

  return ptr;
}

std::unique_ptr<void,std::function<void(void*)>> unwrapCrit(JNIEnv* env, jbyteArray buffer) {
  using namespace std::placeholders;

  std::unique_ptr<void,std::function<void(void*)>> ptr(
    env->GetPrimitiveArrayCritical(buffer, nullptr),
    std::bind(&JNIEnv::ReleasePrimitiveArrayCritical, env, buffer, _1, 0)
  );

  if (!ptr) {
    // OutOfMemoryError already thrown
    throw PendingJavaException();
  }

  return ptr;
}

std::unique_ptr<jbyte,std::function<void(jbyte*)>> unwrap(JNIEnv* env, jbyteArray buffer) {
  using namespace std::placeholders;

  std::unique_ptr<jbyte,std::function<void(jbyte*)>> ptr(
    env->GetByteArrayElements(buffer, nullptr),
    std::bind(&JNIEnv::ReleaseByteArrayElements, env, buffer, _1, JNI_ABORT)
  );

  if (!ptr) {
    // OutOfMemoryError already thrown
    throw PendingJavaException();
  }

  return ptr;
}
    

JNIEXPORT jlong JNICALL Java_com_lightboxtechnologies_lightgrep_PatternHandle_create(JNIEnv*, jclass) {
  return reinterpret_cast<jlong>(lg_create_pattern());
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_PatternHandle_destroy(JNIEnv* env, jobject hPattern) {
  LG_HPATTERN ptr = reinterpret_cast<LG_HPATTERN>(
    env->GetLongField(hPattern, handlePointerField)
  );

  if (ptr) {
    lg_destroy_pattern(ptr);
    env->SetLongField(hPattern, handlePointerField, 0);
  }
}

JNIEXPORT jint JNICALL Java_com_lightboxtechnologies_lightgrep_PatternHandle_parsePatternImpl(JNIEnv* env, jobject hPattern, jstring pattern, jobject options) {
  LG_Error* err = nullptr;
  try {
    // convert all of the Java objects to C
    LG_HPATTERN ptr = reinterpret_cast<LG_HPATTERN>(
      env->GetLongField(hPattern, handlePointerField)
    );

// TODO: C++11 stuff.

    std::unique_ptr<const char,std::function<void(const char*)>> pat(unwrap(env, pattern));

    LG_KeyOptions opts{
      env->GetBooleanField(options, keyOptionsFixedStringField) != 0,
      env->GetBooleanField(options, keyOptionsCaseInsensitiveField) != 0
    };

    // finally actually do something
    const int ret = lg_parse_pattern(
      ptr,
      pat.get(),
      &opts,
      &err
    );
    if (err) {
      throwException(env, keywordExceptionClassName, err->Message);
    }

    return ret;
  }
  catch (const PendingJavaException&) {
    lg_free_error(err);
    return 0;
  }
}

JNIEXPORT jlong JNICALL Java_com_lightboxtechnologies_lightgrep_PatternMapHandle_create(JNIEnv*, jclass, jint numTotalPatternsSizeHint) {
  return reinterpret_cast<jlong>(lg_create_pattern_map(numTotalPatternsSizeHint));
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_PatternMapHandle_destroy(JNIEnv* env, jobject hPatternMap) {
// TODO: template this
  LG_HPATTERNMAP ptr = reinterpret_cast<LG_HPATTERNMAP>(
    env->GetLongField(hPatternMap, handlePointerField)
  );
  if (ptr) {
    lg_destroy_pattern_map(ptr);
    env->SetLongField(hPatternMap, handlePointerField, 0);
  }
}

JNIEXPORT jlong JNICALL Java_com_lightboxtechnologies_lightgrep_FSMHandle_create(JNIEnv*, jclass, jint numFsmStateSizeHint) {
  return reinterpret_cast<jlong>(lg_create_fsm(numFsmStateSizeHint));
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_FSMHandle_destroy(JNIEnv* env, jobject hFsm) {
// TODO: template this
  LG_HFSM ptr = reinterpret_cast<LG_HFSM>(
    env->GetLongField(hFsm, handlePointerField)
  );
  if (ptr) {
    lg_destroy_fsm(ptr);
    env->SetLongField(hFsm, handlePointerField, 0);
  }
}

JNIEXPORT jint JNICALL Java_com_lightboxtechnologies_lightgrep_FSMHandle_addPatternImpl(JNIEnv* env, jobject hFsm, jobject hMap, jobject hPattern, jstring encoding) {
  LG_Error* err = nullptr;
  try {
    // convert all of the Java objects to C
    LG_HFSM fsmptr = reinterpret_cast<LG_HFSM>(
      env->GetLongField(hFsm, handlePointerField)
    );

    LG_HPATTERNMAP mapptr = reinterpret_cast<LG_HPATTERNMAP>(
      env->GetLongField(hMap, handlePointerField)
    );

    LG_HPATTERN patptr = reinterpret_cast<LG_HPATTERN>(
      env->GetLongField(hPattern, handlePointerField)
    );

    std::unique_ptr<const char,std::function<void(const char*)>> enc(unwrap(env, encoding));

    // finally actually do something
    const int ret = lg_add_pattern(
      fsmptr,
      mapptr,
      patptr,
      enc.get(),
      &err
    );
    if (err) {
      throwException(env, keywordExceptionClassName, err->Message);
    }

    return ret;
  }
  catch (const PendingJavaException&) {
    lg_free_error(err);
    return 0;
  }
}

static jobject makeProgramHandle(JNIEnv* env, LG_HPROGRAM hProg) {
  jclass cl = env->FindClass(programHandleClassName);
  throwIfException(env);

  jobject obj = env->NewObject(
    cl,
    programHandleCtor,
    reinterpret_cast<jlong>(hProg)
  );
  throwIfException(env);

  return obj;
}

JNIEXPORT jobject JNICALL Java_com_lightboxtechnologies_lightgrep_FSMHandle_createProgramImpl(JNIEnv* env, jobject hFsm, jobject options) {
  try {
    // convert all of the Java objects to C
    LG_HFSM ptr = reinterpret_cast<LG_HFSM>(
      env->GetLongField(hFsm, handlePointerField)
    );

    LG_ProgramOptions opts{
      env->GetBooleanField(options, programOptionsDeterminizeField) != 0
    };

    // finally actually do something
    LG_HPROGRAM hProg = lg_create_program(ptr, &opts);
    if (!hProg) {
// FIXME: do something else here
//      throwException(env, keywordExceptionClassName, lg_error(hProg));
    }
    return makeProgramHandle(env, hProg);
  }
  catch (const PendingJavaException&) {
    return nullptr;
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_destroy(JNIEnv* env, jobject hProg) {
  LG_HPROGRAM ptr = reinterpret_cast<LG_HPROGRAM>(
    env->GetLongField(hProg, handlePointerField)
  );
  if (ptr) {
    lg_destroy_program(ptr);
    env->SetLongField(hProg, handlePointerField, 0);
  }
}

JNIEXPORT jint JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_sizeImpl(JNIEnv* env, jobject hProg) {
  try {
    LG_HPROGRAM ptr = reinterpret_cast<LG_HPROGRAM>(
      env->GetLongField(hProg, handlePointerField)
    );
    return lg_program_size(ptr);
  }
  catch (const PendingJavaException&) {
    return -1;
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_writeImpl(JNIEnv* env, jobject hProg, jbyteArray buffer, jint offset) {
  try {
    // convert all of the Java objects to C
    LG_HPROGRAM ptr = reinterpret_cast<LG_HPROGRAM>(
      env->GetLongField(hProg, handlePointerField)
    );

    std::unique_ptr<void,std::function<void(void*)>> data(unwrapCrit(env, buffer));

    char* buf = reinterpret_cast<char*>(data.get()) + (uint32_t) offset;

    // finally actually do something
    lg_write_program(ptr, buf);
  }
  catch (const PendingJavaException&) {
  }
}

JNIEXPORT jobject JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_readImpl(JNIEnv* env, jclass, jbyteArray buffer, jint offset, jint size) {
  try {
    LG_HPROGRAM hProg = nullptr;

    {
      // convert all of the Java objects to C
      std::unique_ptr<void,std::function<void(void*)>> data(unwrapCrit(env, buffer));

      char* buf = reinterpret_cast<char*>(data.get()) + (uint32_t) offset;

      // finally actually do something
      hProg = lg_read_program(buf, (uint32_t) size);
// FIXME: handle the error!
/*
      if (!lg_ok(hProg)) {
        throwException(env, programExceptionClassName, lg_error(hProg));
      }
*/
    }

    return makeProgramHandle(env, hProg);
  }
  catch (const PendingJavaException&) {
    return nullptr;
  }
}

static jobject makeContextHandle(JNIEnv* env, LG_HCONTEXT hCtx) {
  jclass cl = env->FindClass(contextHandleClassName);
  throwIfException(env);
  
  jobject obj = env->NewObject(
    cl,
    contextHandleCtor,
    reinterpret_cast<jlong>(hCtx)
  );
  throwIfException(env);

  return obj;
}

JNIEXPORT jobject JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_createContextImpl(JNIEnv* env, jobject hProg, jobject) {
  try {
    // convert all of the Java objects to C
    LG_HPROGRAM ptr = reinterpret_cast<LG_HPROGRAM>(
      env->GetLongField(hProg, handlePointerField)
    );

    LG_ContextOptions opts{
      std::numeric_limits<uint64_t>::max(),
      std::numeric_limits<uint64_t>::max()
    };

    LG_HCONTEXT hCtx = lg_create_context(ptr, &opts);
// FIXME: handle the error!
/*
    if (!lg_ok(hCtx)) {
      throwException(env, programExceptionClassName, lg_error(hCtx));
    }
*/
    return makeContextHandle(env, hCtx);
  }
  catch (const PendingJavaException&) {
    return nullptr;
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_destroy(JNIEnv* env, jobject hCtx) {
  LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
     env->GetLongField(hCtx, handlePointerField)
  );
  if (ptr) {
    lg_destroy_context(ptr);
    env->SetLongField(hCtx, handlePointerField, 0);
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_resetImpl(JNIEnv* env, jobject hCtx) {
  try {
    LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
      env->GetLongField(hCtx, handlePointerField)
    );
    lg_reset_context(ptr);
  }
  catch (const PendingJavaException&) {
  }
}

static void callbackShim(void* userData, const LG_SearchHit* const hit) {
  // NB: Throwing C++ exceptions here is super wacky, as they pass into
  // and then back out of our C API, finally being caught in our wrappers
  // for lg_search or lg_starts_with. This works due to our having no
  // exception_trap in those two C API functions.

  JNIEnv* env;
  jobject cb;
  std::tie(env, cb) = *reinterpret_cast<std::tuple<JNIEnv*,jobject>*>(userData);

  jobject hobj = env->NewObject(
    searchHitClass,
    searchHitCtor,
    (jlong) hit->Start,
    (jlong) hit->End,
    (jint) hit->KeywordIndex
  );
  throwIfException(env);

  env->CallVoidMethod(cb, hitCallbackCallback, hobj);
  throwIfException(env);
}

template <typename F>
static int search(JNIEnv* env, jobject hCtx, F bufGetter, jint offset, jint size, jlong startOffset, jobject callback, F func) {
  try {
    // convert all of the Java objects to C
    LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
      env->GetLongField(hCtx, handlePointerField)
    );

/*
    using namespace std::placeholders;

    std::unique_ptr<jbyte,std::function<void(jbyte*)>> data(
      env->GetByteArrayElements(buffer, nullptr),
      std::bind(&JNIEnv::ReleaseByteArrayElements, env, buffer, _1, JNI_ABORT)
    );

    if (!data) {
      // OutOfMemoryError already thrown
      throw PendingJavaException();
    }
    
    const char* buf = reinterpret_cast<const char*>(data.get()) + offset;
*/

    const char* buf = bufGetter() + offset;

    std::tuple<JNIEnv*,jobject> userData{env, callback};

    // finally actually do something
    return lg_search(
      ptr,
      buf,
      buf + size,
      (uint64_t) startOffset,
      &userData,
      callbackShim
    );
  }
  catch (const PendingJavaException&) {
    return 0;
  }
}

JNIEXPORT jint JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_searchImpl___3BIIJLcom_lightboxtechnologies_lightgrep_HitCallback_2(JNIEnv* env, jobject hCtx, jbyteArray buffer, jint offset, jint size, jlong startOffset, jobject callback) {
  try {
    // convert all of the Java objects to C
    LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
      env->GetLongField(hCtx, handlePointerField)
    );

    std::unique_ptr<jbyte,std::function<void(jbyte*)>> data(unwrap(env, buffer));

    const char* buf = reinterpret_cast<const char*>(data.get()) + offset;
 
    std::tuple<JNIEnv*,jobject> userData{env, callback};

    // finally actually do something
    return lg_search(
      ptr,
      buf,
      buf + size,
      (uint64_t) startOffset,
      &userData,
      callbackShim
    );
  }
  catch (const PendingJavaException&) {
    return 0;
  }
}

JNIEXPORT jint JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_searchImpl__Ljava_nio_ByteBuffer_2IIJLcom_lightboxtechnologies_lightgrep_HitCallback_2(JNIEnv* env, jobject hCtx, jobject buffer, jint offset, jint size, jlong startOffset, jobject callback) {
  try {
    // convert all of the Java objects to C
    LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
      env->GetLongField(hCtx, handlePointerField)
    );

    const char* buf = reinterpret_cast<const char*>(
      env->GetDirectBufferAddress(buffer)
    );

    if (!buf) {
// FIXME: what to do here?
    }
    
    buf += offset;
 
    std::tuple<JNIEnv*,jobject> userData{env, callback};

    // finally actually do something
    return lg_search(
      ptr,
      buf,
      buf + size,
      (uint64_t) startOffset,
      &userData,
      callbackShim
    );
  }
  catch (const PendingJavaException&) {
    return 0;
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_closeoutSearchImpl(JNIEnv* env, jobject hCtx, jobject callback) {
  try {
    // convert all of the Java objects to C
    LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
      env->GetLongField(hCtx, handlePointerField)
    );

    std::tuple<JNIEnv*,jobject> userData{env, callback};

    // finally actually do something
    lg_closeout_search(ptr, &userData, callbackShim);
  }
  catch (const PendingJavaException&) {
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_startsWithImpl___3BIIJLcom_lightboxtechnologies_lightgrep_HitCallback_2(JNIEnv* env, jobject hCtx, jbyteArray buffer, jint offset, jint size, jlong startOffset, jobject callback) {
  try {
    // convert all of the Java objects to C
    LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
      env->GetLongField(hCtx, handlePointerField)
    );

    std::unique_ptr<jbyte,std::function<void(jbyte*)>> data(unwrap(env, buffer));

    const char* buf = reinterpret_cast<const char*>(data.get()) + offset;

    std::tuple<JNIEnv*,jobject> userData{env, callback};

    // finally actually do something
    lg_starts_with(
      ptr,
      buf,
      buf + size,
      (uint64_t) startOffset,
      &userData,
      callbackShim
    );
  }
  catch (const PendingJavaException&) {
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_startsWithImpl__Ljava_nio_ByteBuffer_2IIJLcom_lightboxtechnologies_lightgrep_HitCallback_2(JNIEnv* env, jobject hCtx, jobject buffer, jint offset, jint size, jlong startOffset, jobject callback) {
  try {
    // convert all of the Java objects to C
    LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
      env->GetLongField(hCtx, handlePointerField)
    );

    const char* buf = reinterpret_cast<const char*>(
      env->GetDirectBufferAddress(buffer)
    );

    if (!buf) {
// FIXME: what to do here?
    }
    
    buf += offset;

    std::tuple<JNIEnv*,jobject> userData{env, callback};

    // finally actually do something
    lg_starts_with(
      ptr,
      buf,
      buf + size,
      (uint64_t) startOffset,
      &userData,
      callbackShim
    );
  }
  catch (const PendingJavaException&) {
  }
}

/*
JNIEXPORT jstring JNICALL Java_com_lightboxtechnologies_lightgrep_Lightgrep_lg_1error(JNIEnv* env, jclass, jobject handle) {
  jlong ptr = env->GetLongField(handle, handleField); 
  return env->NewStringUTF(lg_error(reinterpret_cast<void*>(ptr)));
}
*/
