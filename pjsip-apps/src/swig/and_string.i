#if defined(SWIGJAVA) && defined(__ANDROID__)
%typemap(out) std::string
%{
    jsize $1_len = (jsize)$1.length();
    jbyteArray $1_bytes = jenv->NewByteArray($1_len);
    jenv->SetByteArrayRegion($1_bytes, 0, $1_len, (jbyte *) $1.c_str());
    jclass $1_strClass = jenv->FindClass("java/lang/String");
    jmethodID $1_ctorID = jenv->GetMethodID($1_strClass, "<init>", "([BLjava/lang/String;)V");
    jstring $1_encoding = jenv->NewStringUTF("UTF-8");
    jstring $1_jstr = (jstring) jenv->NewObject($1_strClass, $1_ctorID, $1_bytes, $1_encoding);
    jenv->DeleteLocalRef($1_encoding);
    jenv->DeleteLocalRef($1_bytes);
    $result = $1_jstr;
%}

%typemap(directorin,descriptor="Ljava/lang/String;") std::string
%{
    jsize $1_len = (jsize)$1.length();
    jbyteArray $1_bytes = jenv->NewByteArray($1_len);
    jenv->SetByteArrayRegion($1_bytes, 0, $1_len, (jbyte *) $1.c_str());
    jclass $1_strClass = jenv->FindClass("java/lang/String");
    jmethodID $1_ctorID = jenv->GetMethodID($1_strClass, "<init>", "([BLjava/lang/String;)V");
    jstring $1_encoding = jenv->NewStringUTF("UTF-8");
    jstring $1_jstr = (jstring) jenv->NewObject($1_strClass, $1_ctorID, $1_bytes, $1_encoding);
    jenv->DeleteLocalRef($1_encoding);
    jenv->DeleteLocalRef($1_bytes);
    $input = $1_jstr;
%}

%typemap(directorin,descriptor="Ljava/lang/String;") const std::string &
%{
    jsize $1_len = (jsize)$1.length();
    jbyteArray $1_bytes = jenv->NewByteArray($1_len);
    jenv->SetByteArrayRegion($1_bytes, 0, $1_len, (jbyte *) $1.c_str());
    jclass $1_strClass = jenv->FindClass("java/lang/String");
    jmethodID $1_ctorID = jenv->GetMethodID($1_strClass, "<init>", "([BLjava/lang/String;)V");
    jstring $1_encoding = jenv->NewStringUTF("UTF-8");
    jstring $1_jstr = (jstring) jenv->NewObject($1_strClass, $1_ctorID, $1_bytes, $1_encoding);
    jenv->DeleteLocalRef($1_encoding);
    jenv->DeleteLocalRef($1_bytes);
    $input = $1_jstr;
%}

%typemap(out) const std::string &
%{
    jsize $1_len = (jsize)$1->length();
    jbyteArray $1_bytes = jenv->NewByteArray($1_len);
    jenv->SetByteArrayRegion($1_bytes, 0, $1_len, (jbyte *) $1->c_str());
    jclass $1_strClass = jenv->FindClass("java/lang/String");
    jmethodID $1_ctorID = jenv->GetMethodID($1_strClass, "<init>", "([BLjava/lang/String;)V");
    jstring $1_encoding = jenv->NewStringUTF("UTF-8");
    jstring $1_jstr = (jstring) jenv->NewObject($1_strClass, $1_ctorID, $1_bytes, $1_encoding);
    jenv->DeleteLocalRef($1_encoding);
    jenv->DeleteLocalRef($1_bytes);
    $result = $1_jstr;
%}

%typemap(in) std::string 
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
     return $null;
    }
    const jclass $1_strClass = jenv->GetObjectClass($input);
    const jmethodID $1_getBytes = jenv->GetMethodID($1_strClass, "getBytes", "(Ljava/lang/String;)[B");
    const jbyteArray $1_strJbytes = (jbyteArray) jenv->CallObjectMethod($input, $1_getBytes, jenv->NewStringUTF("UTF-8"));

    size_t $1_length = (size_t) jenv->GetArrayLength($1_strJbytes);
    jbyte* $1_pBytes = jenv->GetByteArrayElements($1_strJbytes, NULL);

    if (!$1_pBytes) return $null;
    $1.assign((char *)$1_pBytes, $1_length);
    jenv->ReleaseByteArrayElements($1_strJbytes, $1_pBytes, JNI_ABORT);
    jenv->DeleteLocalRef($1_strJbytes);
    jenv->DeleteLocalRef($1_strClass);
%}

%typemap(directorout) std::string 
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
     return $null;
   }
   const jclass $1_strClass = jenv->GetObjectClass($input);
   const jmethodID $1_getBytes = jenv->GetMethodID($1_strClass, "getBytes", "(Ljava/lang/String;)[B");
   const jbyteArray $1_strJbytes = (jbyteArray) jenv->CallObjectMethod($input, $1_getBytes, jenv->NewStringUTF("UTF-8"));

   size_t $1_length = (size_t) jenv->GetArrayLength($1_strJbytes);
   jbyte* $1_pBytes = jenv->GetByteArrayElements($1_strJbytes, NULL);

   if (!$1_pBytes) return $null;
   $result.assign((char *)$1_pBytes, $1_length);
   jenv->ReleaseByteArrayElements($1_strJbytes, $1_pBytes, JNI_ABORT);
   jenv->DeleteLocalRef($1_strJbytes);
   jenv->DeleteLocalRef($1_strClass);       
%}

%typemap(in) const std::string &
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
     return $null;
   }
   const jclass $1_strClass = jenv->GetObjectClass($input);
   const jmethodID $1_getBytes = jenv->GetMethodID($1_strClass, "getBytes", "(Ljava/lang/String;)[B");
   const jbyteArray $1_strJbytes = (jbyteArray) jenv->CallObjectMethod($input, $1_getBytes, jenv->NewStringUTF("UTF-8"));

   size_t $1_length = (size_t) jenv->GetArrayLength($1_strJbytes);
   jbyte* $1_pBytes = jenv->GetByteArrayElements($1_strJbytes, NULL);

   if (!$1_pBytes) return $null;
   $*1_ltype $1_str((char *)$1_pBytes, $1_length);
   $1 = &$1_str;   
   jenv->ReleaseByteArrayElements($1_strJbytes, $1_pBytes, JNI_ABORT);
   jenv->DeleteLocalRef($1_strJbytes);
   jenv->DeleteLocalRef($1_strClass);
%}

%typemap(directorout,warning=SWIGWARN_TYPEMAP_THREAD_UNSAFE_MSG) const std::string &
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
     return $null;
   }
   const jclass $1_strClass = jenv->GetObjectClass($input);
   const jmethodID $1_getBytes = jenv->GetMethodID($1_strClass, "getBytes", "(Ljava/lang/String;)[B");
   const jbyteArray $1_strJbytes = (jbyteArray) jenv->CallObjectMethod($input, $1_getBytes, jenv->NewStringUTF("UTF-8"));

   size_t $1_length = (size_t) jenv->GetArrayLength($1_strJbytes);
   jbyte* $1_pBytes = jenv->GetByteArrayElements($1_strJbytes, NULL);

   if (!$1_pBytes) return $null;
   /* possible thread/reentrant code problem */
   static $*1_ltype $1_str;
   $1_str = (char *)$1_pBytes;
   $result = &$1_str;
   jenv->ReleaseByteArrayElements($1_strJbytes, $1_pBytes, JNI_ABORT);
   jenv->DeleteLocalRef($1_strJbytes);
   jenv->DeleteLocalRef($1_strClass);
%}

#endif