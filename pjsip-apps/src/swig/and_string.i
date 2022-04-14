#if defined(SWIGJAVA) && defined(__ANDROID__)
%typemap(out) std::string
%{
    jsize $1_len = (jsize)$1.length();
    jbyteArray bytes = jenv->NewByteArray($1_len);
    jenv->SetByteArrayRegion(bytes, 0, $1_len, (jbyte *) $1.c_str());
    jclass strClass = jenv->FindClass("java/lang/String");
    jmethodID ctorID = jenv->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jstring encoding = jenv->NewStringUTF("UTF-8");
    jstring jstr = (jstring) jenv->NewObject(strClass, ctorID, bytes, encoding);
    jenv->DeleteLocalRef(encoding);
    jenv->DeleteLocalRef(bytes);
    $result = jstr;
%}

%typemap(directorin,descriptor="Ljava/lang/String;") std::string
%{
    jsize $1_len = (jsize)$1.length();
    jbyteArray bytes = jenv->NewByteArray($1_len);
    jenv->SetByteArrayRegion(bytes, 0, $1_len, (jbyte *) $1.c_str());
    jclass strClass = jenv->FindClass("java/lang/String");
    jmethodID ctorID = jenv->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jstring encoding = jenv->NewStringUTF("UTF-8");
    jstring jstr = (jstring) jenv->NewObject(strClass, ctorID, bytes, encoding);
    jenv->DeleteLocalRef(encoding);
    jenv->DeleteLocalRef(bytes);
    $input = jstr;
%}

%typemap(directorin,descriptor="Ljava/lang/String;") const std::string &
%{
    jsize $1_len = (jsize)$1.length();
    jbyteArray bytes = jenv->NewByteArray($1_len);
    jenv->SetByteArrayRegion(bytes, 0, $1_len, (jbyte *) $1.c_str());
    jclass strClass = jenv->FindClass("java/lang/String");
    jmethodID ctorID = jenv->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jstring encoding = jenv->NewStringUTF("UTF-8");
    jstring jstr = (jstring) jenv->NewObject(strClass, ctorID, bytes, encoding);
    jenv->DeleteLocalRef(encoding);
    jenv->DeleteLocalRef(bytes);
    $input = jstr;
%}

%typemap(out) const std::string &
%{
    jsize $1_len = (jsize)$1->length();
    jbyteArray bytes = jenv->NewByteArray($1_len);
    jenv->SetByteArrayRegion(bytes, 0, $1_len, (jbyte *) $1->c_str());
    jclass strClass = jenv->FindClass("java/lang/String");
    jmethodID ctorID = jenv->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jstring encoding = jenv->NewStringUTF("UTF-8");
    jstring jstr = (jstring) jenv->NewObject(strClass, ctorID, bytes, encoding);
    jenv->DeleteLocalRef(encoding);
    jenv->DeleteLocalRef(bytes);
    $result = jstr;
%}
#endif