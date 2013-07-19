# $Id: my_typemaps.i 4566 2013-07-17 20:20:50Z nanang $

%include "arrays_java.i"
%include "typemaps.i"

/* Auto destroy JNI object, useful for director */
%header %{
class LocalRefGuard {
    JNIEnv* jenv;
    jobject jobj;
public:
    LocalRefGuard(JNIEnv* jenv_, jobject jobj_) : jenv(jenv_), jobj(jobj_) {}
    ~LocalRefGuard() { if (jobj) jenv->DeleteLocalRef(jobj); }
};
%}

/*
 * Typemap for setting member of inner struct/union.
 */
%typemap(memberin) NESTED_INNER "if ($input) pj_memcpy(&$1, $input, sizeof(*$input));"


/*
 * Add director typemaps for "int *INOUT"
 */
%typemap(javadirectorin) int *INOUT "$jniinput"
%typemap(directorin, descriptor="[I") int *INOUT %{
  $input = jenv->NewIntArray(1);
  LocalRefGuard g_$input(jenv, $input);
  jenv->SetIntArrayRegion($input, 0, 1, (jint*)$1);
%}
%typemap(directorout) int *INOUT %{
  jint tmp$result;
  jenv->GetIntArrayRegion($input, 0, 1, &tmp$result);
  *$result = ($*1_ltype)tmp$result;
%}
%typemap(directorargout) int *INOUT %{
  jint tmp$1;
  jenv->GetIntArrayRegion($input, 0, 1, &tmp$1);
  *$1 = ($*1_ltype)tmp$1;
%}

/*
 * Add director typemaps for "unsigned *INOUT"
 */
%typemap(javadirectorin) unsigned *INOUT "$jniinput"
%typemap(directorin, descriptor="[I") unsigned *INOUT %{
  $input = jenv->NewLongArray(1);
  LocalRefGuard g_$input(jenv, $input);
  jenv->SetLongArrayRegion($input, 0, 1, (jlong*)$1);
%}
%typemap(directorout) unsigned *INOUT %{
  jlong tmp$result;
  jenv->GetLongArrayRegion($input, 0, 1, &tmp$result);
  *$result = ($*1_ltype)tmp$result;
%}
%typemap(directorargout) unsigned *INOUT %{
  jlong tmp$1;
  jenv->GetLongArrayRegion($input, 0, 1, &tmp$1);
  *$1 = ($*1_ltype)tmp$1;
%}

/*
 * Map pj_str_t to java String
 */
%ignore pj_str_t;
%typemap(jni)             pj_str_t* "jstring"
%typemap(jtype)           pj_str_t* "String"
%typemap(jstype)          pj_str_t* "String"
%typemap(javain)          pj_str_t* "$javainput"
%typemap(javaout)         pj_str_t* { return $jnicall; }
%typemap(javadirectorin)  pj_str_t* "$jniinput"
%typemap(javadirectorout) pj_str_t* "$javacall"

%typemap(in)              pj_str_t* (pj_str_t str_tmp, char* str_ptr=NULL) %{
  $1 = &str_tmp;
  if(!$input) pj_strset($1, NULL, 0);
  else {
    str_ptr = (char*)jenv->GetStringUTFChars($input, 0);
    pj_size_t str_len = jenv->GetStringUTFLength($input);
    if (!str_ptr || !str_len) pj_strset($1, NULL, 0);
    else pj_strset($1, str_ptr, str_len);
  }
%}

%typemap(freearg)         pj_str_t* %{
  if ($input)
    jenv->ReleaseStringUTFChars($input, str_ptr$argnum);
%}

%typemap(memberin)        pj_str_t %{
  if ($1.ptr) free($1.ptr);
  if ($input->ptr) {
    $1.ptr = (char*)malloc($input->slen);				/* <-- will leak! */
    memcpy($1.ptr, $input->ptr, $input->slen);
    $1.slen = $input->slen;
  }
%}

%typemap(out)             pj_str_t* %{
  char *$1_ptr = (char*)malloc($1->slen+1);
  memcpy($1_ptr, $1->ptr, $1->slen);
  $1_ptr[$1->slen] = '\0';
  $result = jenv->NewStringUTF($1_ptr);
  free($1_ptr);
%}

%typemap(directorin,descriptor="Ljava/lang/String;") pj_str_t* %{
  char *$input_ptr = (char*)malloc($1->slen+1);
  memcpy($input_ptr, $1->ptr, $1->slen);
  $input_ptr[$1->slen] = '\0';
  $input = jenv->NewStringUTF($input_ptr);
  LocalRefGuard g_$input(jenv, $input);
  free($input_ptr);
%}

/*
 * Map pj_str_t[ANY] to java String[]
 */
%typemap(jni)     pj_str_t[ANY] "jobjectArray"
%typemap(jtype)	  pj_str_t[ANY] "String[]"
%typemap(jstype)  pj_str_t[ANY] "String[]"
%typemap(javain)  pj_str_t[ANY] "$javainput"
%typemap(javaout) pj_str_t[ANY] { return $jnicall; }
%typemap(in)      pj_str_t[ANY] %{
  int $1_len = jenv->GetArrayLength($input);
  char** $1_tmpstr = new char*[$1_len];
  jstring* $1_tmp = new jstring[$1_len];
  $1 = new pj_str_t[$1_len];
  int $1_ii;
  for ($1_ii = 0; $1_ii<$1_len; $1_ii++) {
    $1_tmp[$1_ii] = (jstring)jenv->GetObjectArrayElement($input, $1_ii);
    if ($1_tmp[$1_ii]) {
      $1[$1_ii].ptr = $1_tmpstr[$1_ii] = (char*)jenv->GetStringUTFChars($1_tmp[$1_ii], 0);
      $1[$1_ii].slen = jenv->GetStringUTFLength($1_tmp[$1_ii]);
    } else {
      pj_strset(&$1[$1_ii], NULL, 0);
    }
    jenv->DeleteLocalRef($1_tmp[$1_ii]); /* is this ok here? */
  }
%}

%typemap(freearg) pj_str_t[ANY] %{
  for ($1_ii=0; $1_ii<$1_len; $1_ii++)
    if ($1_tmp[$1_ii]) jenv->ReleaseStringUTFChars($1_tmp[$1_ii], $1_tmpstr[$1_ii]);
  delete [] $1;
  delete [] $1_tmp;
  delete [] $1_tmpstr;
%}

/* 
 * Handle setter/getter of a struct member with type of 'array of pj_str_t'.
 */
%define MY_JAVA_MEMBER_ARRAY_OF_STR(CLASS_NAME, NAME, COUNT_NAME)

%ignore CLASS_NAME::COUNT_NAME;

%typemap(memberin)	pj_str_t NAME[ANY], pj_str_t NAME[] {
  int i;
  for (i=0; i<arg1->COUNT_NAME; ++i)
    if ($1[i].ptr) free($1[i].ptr);
  
  arg1->COUNT_NAME = $input_len;
  for (i=0; i<arg1->COUNT_NAME; ++i) {
    if ($input->ptr) {
      $1[i].ptr = (char*)malloc($input->slen);				/* <-- will leak! */
      memcpy($1[i].ptr, $input->ptr, $input->slen);
      $1[i].slen = $input->slen;
    }
  }
}

%typemap(out) 		pj_str_t NAME[ANY] {
  int i;
  jstring temp_string;
  const jclass clazz = jenv->FindClass("java/lang/String");

  $result = jenv->NewObjectArray(arg1->COUNT_NAME, clazz, NULL);
  for (i=0; i<arg1->COUNT_NAME; i++) {
    temp_string = jenv->NewStringUTF($1[i].ptr);
    jenv->SetObjectArrayElement($result, i, temp_string);
    jenv->DeleteLocalRef(temp_string);
  }
}

%enddef

/*
 * pj_str_t *INOUT, string as input and output param, handled as single element array.
 */
%apply pj_str_t[ANY] { pj_str_t *INOUT };

%typemap(argout) pj_str_t *INOUT {
  char *$1_ptr = new char[$1[0].slen+1];
  memcpy($1_ptr, $1[0].ptr, $1[0].slen);
  $1_ptr[$1[0].slen] = '\0';
  jstring temp_string = jenv->NewStringUTF($1_ptr);
  jenv->SetObjectArrayElement($input, 0, temp_string);
  delete [] $1_ptr;
}

%typemap(directorin,descriptor="[Ljava/lang/String;") pj_str_t *INOUT %{
  jstring tmp_$input;
  $input = jenv->NewObjectArray(1, jenv->FindClass("java/lang/String"), NULL);
  LocalRefGuard g_$input(jenv, $input);
  tmp_$input = jenv->NewStringUTF($1[0].ptr);
  LocalRefGuard g_temp_$input(jenv, tmp_$input);
  jenv->SetObjectArrayElement($input, 0, tmp_$input);
%}

%typemap(directorargout)  pj_str_t *INOUT {
  if(!$input) pj_strset($1, NULL, 0);
  else {
    str_ptr = (char*)jenv->GetStringUTFChars($input, 0);
    pj_size_t str_len = jenv->GetStringUTFLength($input);
    if (!str_ptr || !str_len) pj_strset($1, NULL, 0);
    else {
      $1->ptr = (char*)malloc(str_len);					/* <-- will leak! */
      memcpy($1->ptr, str_ptr, str_len);
      $1->slen = str_len;
    }
    jenv->ReleaseStringUTFChars($input, str_ptr);
  }
}


/* 
 * Object input & output (with double pointers).
 */
%define MY_JAVA_CLASS_INOUT(TYPE, NAME)
%typemap(jni) 	 TYPE **NAME "jlong"
%typemap(jtype)  TYPE **NAME "long"
%typemap(jstype) TYPE **NAME "TYPE"
%typemap(in) 	 TYPE **NAME (TYPE *temp = NULL) "temp = (TYPE*)$input; $1 = &temp;"
%typemap(argout) TYPE **NAME "$input = (jlong)*$1;"
%typemap(directorin,descriptor="L$packagepath/TYPE;") TYPE **NAME "$input = (jlong)*$1;"
%typemap(directorargout)  TYPE **NAME " *$1 = (TYPE*)$input; "
%typemap(javain)	  TYPE **NAME "TYPE.getCPtr($javainput)"
%typemap(javadirectorin)  TYPE **NAME "($jniinput == 0) ? null : new TYPE($jniinput, false)"
%typemap(javadirectorout) TYPE **NAME "TYPE.getCPtr($javacall)"
%enddef


/* 
 * Generate setter/getter of a struct member typed of 'array of enum' with variable length.
 */
%define MY_JAVA_MEMBER_ARRAY_OF_ENUM(CLASS_NAME, TYPE, NAME, COUNT_NAME)

%ignore CLASS_NAME::COUNT_NAME;
%apply ARRAYSOFENUMS[] { TYPE* NAME };

%typemap(in)       TYPE* NAME (jint *jarr) %{
  int size$1 = jenv->GetArrayLength($input);
  if (!SWIG_JavaArrayInInt(jenv, &jarr, (int **)&$1, $input)) return $null;
%}

%typemap(memberin) TYPE* NAME %{
  if ($1) free($1);
  arg1->COUNT_NAME = size$input;
  $1 = (TYPE*)calloc(arg1->COUNT_NAME, sizeof(TYPE));			/* <-- will leak! */
  for (size_t i = 0; i < (size_t)arg1->COUNT_NAME; i++)
    $1[i] = $input[i];
%}

%typemap(out)      TYPE* NAME %{
  $result = SWIG_JavaArrayOutInt(jenv, (int*)$1, arg1->COUNT_NAME);
%}

// Hack for avoiding leak, may not work when another macro defines class destructor too
%extend CLASS_NAME { ~CLASS_NAME() { if ($self->NAME) free($self->NAME); } };

%enddef


/* 
 * Handle setter/getter of a struct member with type of 'array of pointer to struct/class'.
 */
%define MY_JAVA_MEMBER_ARRAY_OF_POINTER(CLASS_NAME, TYPE, NAME, COUNT_NAME)

%ignore CLASS_NAME::COUNT_NAME;
%typemap(jni) 	   TYPE* NAME[ANY] "jlongArray"
%typemap(jtype)    TYPE* NAME[ANY] "long[]"
%typemap(jstype)   TYPE* NAME[ANY] "TYPE[]"
%typemap(javain)   TYPE* NAME[ANY] "TYPE.cArrayUnwrap($javainput)"
%typemap(javaout)  TYPE* NAME[ANY] {return TYPE.cArrayWrap($jnicall, $owner);}

%typemap(in)       TYPE* NAME[ANY] (jlong *jarr) %{
  if (!SWIG_JavaArrayInUlong(jenv, &jarr, (unsigned long**)&$1, $input))
    return $null;
  arg1->COUNT_NAME = jenv->GetArrayLength($input);
%}

%typemap(freearg)  TYPE* NAME[ANY] %{ if ($1) delete [] $1; %}

%typemap(memberin) TYPE* NAME[ANY] %{
  for (size_t i = 0; i < (size_t)arg1->COUNT_NAME; i++) $1[i] = $input[i];
%}

%typemap(out)      TYPE* NAME[ANY] %{
  $result = SWIG_JavaArrayOutUlong(jenv, (unsigned long*)$1, arg1->COUNT_NAME);
%}

%typemap(javacode) TYPE %{
  protected static long[] cArrayUnwrap($javaclassname[] arrayWrapper) {
    long[] cArray = new long[arrayWrapper.length];
    for (int i=0; i<arrayWrapper.length; i++)
      cArray[i] = $javaclassname.getCPtr(arrayWrapper[i]);
    return cArray;
  }
  protected static $javaclassname[] cArrayWrap(long[] cArray, boolean cMemoryOwn) {
    $javaclassname[] arrayWrapper = new $javaclassname[cArray.length];
    for (int i=0; i<cArray.length; i++)
      arrayWrapper[i] = new $javaclassname(cArray[i], cMemoryOwn);
    return arrayWrapper;
  }
%}

%enddef
