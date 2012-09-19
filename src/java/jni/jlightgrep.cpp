#include "lightgrep_c_api.h"
#include "jlightgrep.h"

#include <functional>
#include <memory>
#include <sstream>
#include <tuple>

static jfieldID parserHandlePointerField;
static jfieldID programHandlePointerField;
static jfieldID contextHandlePointerField;

static jmethodID programHandleCtor;
static jmethodID contextHandleCtor;

static const char* parserHandleClassName = "com/lightboxtechnologies/lightgrep/ParserHandle";
static const char* programHandleClassName = "com/lightboxtechnologies/lightgrep/ProgramHandle";
static const char* contextHandleClassName = "com/lightboxtechnologies/lightgrep/ContextHandle";

static jfieldID keyOptionsFixedStringField;
static jfieldID keyOptionsCaseInsensitiveField;
static jfieldID programOptionsDeterminizeField;
static jfieldID contextOptionsTraceBeginField;
static jfieldID contextOptionsTraceEndField;

static const char* keyOptionsClassName = "com/lightboxtechnologies/lightgrep/KeyOptions";
static const char* programOptionsClassName = "com/lightboxtechnologies/lightgrep/ProgramOptions";
static const char* contextOptionsClassName = "com/lightboxtechnologies/lightgrep/ContextOptions";

static jmethodID searchHitCtor;
static jfieldID searchHitStartField;
static jfieldID searchHitEndField;
static jfieldID searchHitKeywordIndexField;

static const char* searchHitClassName = "com/lightboxtechnologies/lightgrep/SearchHit";

static jmethodID hitCallbackCallback;

static const char* hitCallbackClassName = "com/lightboxtechnologies/lightgrep/HitCallback";

static const char* nullPointerExceptionClassName = "java/lang/NullPointerException";
static const char* indexOutOfBoundsExceptionClassName = "java/lang/IndexOutOfBoundsException";
static const char* keywordExceptionClassName = "com/lightboxtechnologies/lightgrep/KeywordException";
static const char* programExceptionClassName = "com/lightboxtechnologies/lightgrep/ProgramException";

// FIXME: ack, need to load these in the <cinit> for each class!
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* jvm, void*) {
  JNIEnv* env;
  if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  // cache the field and method IDs we use to prevent lookup every time
  jclass c;

  c = env->FindClass(parserHandleClassName);
  parserHandlePointerField = env->GetFieldID(c, "Pointer", "J");

  c = env->FindClass(programHandleClassName);
  programHandleCtor = env->GetMethodID(c, "<init>", "(J)V"); 
  programHandlePointerField = env->GetFieldID(c, "Pointer", "J");

  c = env->FindClass(contextHandleClassName);
  contextHandleCtor = env->GetMethodID(c, "<init>", "(J)V");
  contextHandlePointerField = env->GetFieldID(c, "Pointer", "J");

  c = env->FindClass(keyOptionsClassName);
  keyOptionsFixedStringField = env->GetFieldID(c, "FixedString", "Z");
  keyOptionsCaseInsensitiveField = env->GetFieldID(c, "FixedString", "Z");

  c = env->FindClass(programOptionsClassName);
  programOptionsDeterminizeField = env->GetFieldID(c, "Determinize", "Z");

  c = env->FindClass(contextOptionsClassName);
  contextOptionsTraceBeginField = env->GetFieldID(c, "TraceBegin", "J");
  contextOptionsTraceBeginField = env->GetFieldID(c, "TraceEnd", "J");

  c = env->FindClass(searchHitClassName);
  searchHitCtor = env->GetMethodID(c, "<init>", "(JJI)V");
  searchHitStartField = env->GetFieldID(c, "Start", "J");
  searchHitEndField = env->GetFieldID(c, "End", "J");
  searchHitKeywordIndexField = env->GetFieldID(c, "KeywordIndex", "I");

  c = env->FindClass(hitCallbackClassName);
  hitCallbackCallback = env->GetMethodID(c, "callback", "(Lcom/lightboxtechnologies/lightgrep/SearchHit;)V");

  return JNI_VERSION_1_6;
}

template <typename V>
bool throwIfNull(JNIEnv* env, const char* varname, const V* var) {
  if (var == nullptr) {
    std::ostringstream ss;
    ss << varname << " == null";

    env->ThrowNew(
      env->FindClass(nullPointerExceptionClassName),
      ss.str().c_str()
    );
    return true;
  }
  else {
    return false;
  }
}

template <typename V>
bool throwIfNegative(JNIEnv* env, const char* varname, V var) {
  if (var < 0) {
    std::ostringstream ss;
    ss << varname << " == " << var << " < 0";

    env->ThrowNew(
      env->FindClass(indexOutOfBoundsExceptionClassName),
      ss.str().c_str()
    );
    return true;
  }
  else {
    return false;
  }
}

template <typename O, typename S>
bool throwIfBufferTooSmall(JNIEnv* env, const char* bufname, jbyteArray buffer, const char* offname, O offset, const char* sname, S size) {
  const jsize buflen = env->GetArrayLength(buffer);
  if (buflen - offset < size) {
    std::ostringstream ss;
    ss << bufname << ".length == " << buflen << ", "
       << offname << " == " << offset << ", "
       << bufname << ".length - " << offname << " < "
       << sname << " == " << size;

    env->ThrowNew(
      env->FindClass(indexOutOfBoundsExceptionClassName),
      ss.str().c_str()
    );
    return true;
  }
  else {
    return false;
  }
}

JNIEXPORT jlong JNICALL Java_com_lightboxtechnologies_lightgrep_ParserHandle_create(JNIEnv*, jclass, jint numFsmStateSizeHint) {
  return reinterpret_cast<jlong>(lg_create_parser(numFsmStateSizeHint));
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ParserHandle_destroy(JNIEnv* env, jobject hParser) {
  jlong ptr = env->GetLongField(hParser, parserHandlePointerField);
  lg_destroy_parser(reinterpret_cast<LG_HPARSER>(ptr));
}

JNIEXPORT jint JNICALL Java_com_lightboxtechnologies_lightgrep_ParserHandle_addKeyword(JNIEnv* env, jobject hParser, jstring keyword, jint keyIndex, jobject options, jstring encoding) {
  // validate all of the arguments so we don't crash the JVM
  if (throwIfNull(env, "keyword", keyword) || 
      throwIfNegative(env, "keyIndex", keyIndex) ||
      throwIfNull(env, "options", options) ||
      throwIfNull(env, "encoding", encoding))
  {
    return 0;
  }

  // convert all of the Java objects to C
  jlong ptr = env->GetLongField(hParser, parserHandlePointerField);

  using namespace std::placeholders;

  std::unique_ptr<const char,std::function<void(const char*)>> kw(
    env->GetStringUTFChars(keyword, nullptr),
    std::bind(&JNIEnv::ReleaseStringUTFChars, env, keyword, _1)
  );

  LG_KeyOptions opts{
    env->GetBooleanField(options, keyOptionsFixedStringField) != 0,
    env->GetBooleanField(options, keyOptionsCaseInsensitiveField) != 0
  };

  std::unique_ptr<const char,std::function<void(const char*)>> enc(
    env->GetStringUTFChars(encoding, nullptr),
    std::bind(&JNIEnv::ReleaseStringUTFChars, env, encoding, _1)
  );

  // finally actually do something
  const int ret = lg_add_keyword(
    reinterpret_cast<LG_HPARSER>(ptr),
    kw.get(),
    std::strlen(kw.get()),
    (uint32) keyIndex,
    &opts,
    enc.get()
  );

  if (!ret) {
    env->ThrowNew(
      env->FindClass(keywordExceptionClassName),
      lg_error(reinterpret_cast<LG_HPARSER>(ptr))
    );
  }

  return ret;
}

JNIEXPORT jobject JNICALL Java_com_lightboxtechnologies_lightgrep_ParserHandle_createProgram(JNIEnv* env, jobject hParser, jobject options) {
  // validate all of the arguments so we don't crash the JVM
  if (throwIfNull(env, "options", options)) {
    return nullptr;
  }
  
  // convert all of the Java objects to C
  LG_HPARSER ptr = reinterpret_cast<LG_HPARSER>(
    env->GetLongField(hParser, parserHandlePointerField)
  );

  LG_ProgramOptions opts{
    env->GetBooleanField(options, programOptionsDeterminizeField) != 0
  };

  // finally actually do something
  LG_HPROGRAM hProg = lg_create_program(ptr, &opts);
// FIXME: don't use lg_ok
  if (lg_ok(hProg)) {
    return env->NewObject(
      env->FindClass(programHandleClassName),
      programHandleCtor,
      reinterpret_cast<jlong>(hProg)
    );
  }
  else {
    env->ThrowNew(
      env->FindClass(keywordExceptionClassName),
      lg_error(reinterpret_cast<LG_HPARSER>(ptr))
    );
    return nullptr;
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_destroy(JNIEnv* env, jobject hProg) {
  jlong ptr = env->GetLongField(hProg, programHandlePointerField);
  lg_destroy_program(reinterpret_cast<LG_HPROGRAM>(ptr));
}

JNIEXPORT jint JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_size(JNIEnv* env, jobject hProg) {
  jlong ptr = env->GetLongField(hProg, programHandlePointerField); 
  return lg_program_size(reinterpret_cast<LG_HPROGRAM>(ptr));
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_write(JNIEnv* env, jobject hProg, jbyteArray buffer, jint offset) {
  // validate all of the arguments so we don't crash the JVM
  if (throwIfNull(env, "buffer", buffer) ||
      throwIfNegative(env, "offset", offset))
  {
    return;
  }

  LG_HPROGRAM ptr = reinterpret_cast<LG_HPROGRAM>(
    env->GetLongField(hProg, programHandlePointerField)
  );

  if (
    throwIfBufferTooSmall(
      env,
      "buffer", buffer,
      "offset", offset,
      "program size", lg_program_size(ptr))
    )
  {
    return;
  }

  // convert all of the Java objects to C
  using namespace std::placeholders;

  std::unique_ptr<void,std::function<void(void*)>> data(
    env->GetPrimitiveArrayCritical(buffer, nullptr),
    std::bind(&JNIEnv::ReleasePrimitiveArrayCritical, env, buffer, _1, 0)
  );

  // finally actually do something
  lg_write_program(ptr, reinterpret_cast<char*>(data.get()) + (uint32) offset);
}

JNIEXPORT jobject JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_read(JNIEnv* env, jclass, jbyteArray buffer, jint offset, jint size) {
  // validate all of the arguments so we don't crash the JVM
  if (throwIfNull(env, "buffer", buffer) ||
      throwIfNegative(env, "offset", offset) ||
      throwIfNegative(env, "size", size) ||
      throwIfBufferTooSmall(
        env,
        "buffer", buffer,
        "offset", offset,
        "size", size
      )
    )
  {
    return nullptr;
  }

  // convert all of the Java objects to C
  using namespace std::placeholders;

  std::unique_ptr<void,std::function<void(void*)>> data(
    env->GetPrimitiveArrayCritical(buffer, nullptr),
    std::bind(&JNIEnv::ReleasePrimitiveArrayCritical, env, buffer, _1, 0)
  );

  char* buf = reinterpret_cast<char*>(data.get()) + (uint32) offset;

  // finally actually do something
  LG_HPROGRAM hProg = lg_read_program(buf, (uint32) size);
  if (lg_ok(hProg)) {
    return env->NewObject(
      env->FindClass(programHandleClassName),
      programHandleCtor,
      reinterpret_cast<jlong>(hProg)
    );
  }
  else {
    env->ThrowNew(
      env->FindClass(programExceptionClassName),
      lg_error(hProg)
    );
    return nullptr;
  }
}

JNIEXPORT jobject JNICALL Java_com_lightboxtechnologies_lightgrep_ProgramHandle_createContext(JNIEnv* env, jobject hProg, jobject options) {
  // validate all of the arguments so we don't crash the JVM
  if (throwIfNull(env, "options", options)) {
    return nullptr;
  }
  
  // convert all of the Java objects to C
  LG_HPROGRAM ptr = reinterpret_cast<LG_HPROGRAM>(
    env->GetLongField(hProg, programHandlePointerField)
  );

  LG_ContextOptions opts{
    (uint64) env->GetLongField(options, contextOptionsTraceBeginField),
    (uint64) env->GetLongField(options, contextOptionsTraceEndField),
  };

  LG_HCONTEXT hCtx = lg_create_context(ptr, &opts);
  if (lg_ok(hCtx)) {
    return env->NewObject(
      env->FindClass(contextHandleClassName),
      contextHandleCtor,
      reinterpret_cast<jlong>(hCtx)
    );
  }
  else {
    env->ThrowNew(
      env->FindClass(programExceptionClassName),
      lg_error(hCtx)
    );
    return nullptr;
  }
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_destroy(JNIEnv* env, jobject hCtx) {
  jlong ptr = env->GetLongField(hCtx, contextHandlePointerField);
  lg_destroy_context(reinterpret_cast<LG_HCONTEXT>(ptr));
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_reset(JNIEnv* env, jobject hCtx) {
  jlong ptr = env->GetLongField(hCtx, contextHandlePointerField); 
  lg_reset_context(reinterpret_cast<LG_HCONTEXT>(ptr));
}

void callbackShim(void* userData, const LG_SearchHit* const hit) {
  JNIEnv* env;
  jobject cb;
  std::tie(env, cb) = *reinterpret_cast<std::tuple<JNIEnv*,jobject>*>(userData);

  jobject hobj = env->NewObject(
    env->FindClass(searchHitClassName),
    searchHitCtor,
    (jlong) hit->Start,
    (jlong) hit->End,
    (jint) hit->KeywordIndex
  );

  env->CallVoidMethod(cb, hitCallbackCallback, hobj);  
}

JNIEXPORT jint JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_search(JNIEnv* env, jobject hCtx, jbyteArray buffer, jint offset, jint size, jlong startOffset, jobject callback) {
  // validate all of the arguments so we don't crash the JVM
  if (throwIfNull(env, "buffer", buffer) ||
      throwIfNegative(env, "offset", offset) ||
      throwIfNegative(env, "size", size) ||
      throwIfBufferTooSmall(
        env,
        "buffer", buffer,
        "offset", offset,
        "size", size
      ) ||
      throwIfNegative(env, "startOffset", startOffset) ||
      throwIfNull(env, "callback", callback)
    )
  {
    return 0;
  }

  // convert all of the Java objects to C
  LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
    env->GetLongField(hCtx, contextHandlePointerField)
  );

  using namespace std::placeholders;

  std::unique_ptr<void,std::function<void(void*)>> data(
    env->GetPrimitiveArrayCritical(buffer, nullptr),
    std::bind(&JNIEnv::ReleasePrimitiveArrayCritical, env, buffer, _1, JNI_ABORT)
  );

// FIXME: check for OOM. Wtf do we do then?
    
  const char* buf = reinterpret_cast<const char*>(data.get()) + offset;
 
  std::tuple<JNIEnv*,jobject> userData{env, callback};

  // finally actually do something
  return lg_search(
    ptr,
    buf,
    buf + size,
    (uint64) startOffset,
    &userData,
    callbackShim
  );
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_closeoutSearch(JNIEnv* env, jobject hCtx, jobject callback) {
  // validate all of the arguments so we don't crash the JVM
  if (throwIfNull(env, "callback", callback)) {
    return;
  }

  // convert all of the Java objects to C
  LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
    env->GetLongField(hCtx, contextHandlePointerField)
  );

  std::tuple<JNIEnv*,jobject> userData{env, callback};

  // finally actually do something
  lg_closeout_search(ptr, &userData, callbackShim);
}

JNIEXPORT void JNICALL Java_com_lightboxtechnologies_lightgrep_ContextHandle_startsWith(JNIEnv* env, jobject hCtx, jbyteArray buffer, jint offset, jint size, jlong startOffset, jobject callback) {
  // validate all of the arguments so we don't crash the JVM
  if (throwIfNull(env, "buffer", buffer) ||
      throwIfNegative(env, "offset", offset) ||
      throwIfNegative(env, "size", size) ||
      throwIfBufferTooSmall(
        env,
        "buffer", buffer,
        "offset", offset,
        "size", size
      ) ||
      throwIfNegative(env, "startOffset", startOffset) ||
      throwIfNull(env, "callback", callback)
    )
  {
    return;
  }

  // convert all of the Java objects to C
  LG_HCONTEXT ptr = reinterpret_cast<LG_HCONTEXT>(
    env->GetLongField(hCtx, contextHandlePointerField)
  );

  using namespace std::placeholders;

  std::unique_ptr<void,std::function<void(void*)>> data(
    env->GetPrimitiveArrayCritical(buffer, nullptr),
    std::bind(&JNIEnv::ReleasePrimitiveArrayCritical, env, buffer, _1, JNI_ABORT)
  );

// FIXME: check for OOM. Wtf do we do then?

  const char* buf = reinterpret_cast<const char*>(data.get()) + offset;

  std::tuple<JNIEnv*,jobject> userData{env, callback};

  // finally actually do something
  lg_starts_with(
    ptr,
    buf,
    buf + size,
    (uint64) startOffset,
    &userData,
    callbackShim
  );
}

/*
JNIEXPORT jstring JNICALL Java_com_lightboxtechnologies_lightgrep_Lightgrep_lg_1error(JNIEnv* env, jclass, jobject handle) {
  jlong ptr = env->GetLongField(handle, handleField); 
  return env->NewStringUTF(lg_error(reinterpret_cast<void*>(ptr)));
}
*/
