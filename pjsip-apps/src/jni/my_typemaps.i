# $Id$

%include "arrays_java.i"
%include "typemaps.i"

/* Add director typemaps for "int *INOUT" */
%typemap(javadirectorin) int *INOUT "$jniinput"
%typemap(directorin, descriptor="[I") int *INOUT %{
  $input = jenv->NewIntArray(1);
  jenv->SetIntArrayRegion($input, 0, 1, (jint*)$1);
%}
%typemap(directorout) int *INOUT %{
  jint* tmp$result = jenv->GetIntArrayElements($input, 0);
  *$result = tmp$result[0];
  jenv->ReleaseIntArrayElements($input, tmp$result, JNI_ABORT);
%}
%typemap(directorargout) int *INOUT %{
  jint* tmp$1 = jenv->GetIntArrayElements($input, 0);
  *$1 = ($*1_ltype) tmp$1[0];
  jenv->ReleaseIntArrayElements($input, tmp$1, JNI_ABORT);
%}

/* Add director typemaps for "unsigned *INOUT" */
%typemap(javadirectorin) unsigned *INOUT "$jniinput"
%typemap(directorin, descriptor="[I") unsigned *INOUT %{
  $input = jenv->NewLongArray(1);
  jenv->SetLongArrayRegion($input, 0, 1, (jlong*)$1);
%}
%typemap(directorout) unsigned *INOUT %{
  jlong* tmp$result = jenv->GetLongArrayElements($input, 0);
  *$result = tmp$result[0];
  jenv->ReleaseLongArrayElements($input, tmp$result, JNI_ABORT);
%}
%typemap(directorargout) unsigned *INOUT %{
  jlong* tmp$1 = jenv->GetLongArrayElements($input, 0);
  *$1 = ($*1_ltype) tmp$1[0];
  jenv->ReleaseLongArrayElements($input, tmp$1, JNI_ABORT);
%}

/* 
 * Object input & output (with double pointers).
 */
%define MY_JAVA_CLASS_INOUT(TYPE, NAME)
%typemap(jni) 	 TYPE **NAME "jlong"
%typemap(jtype)  TYPE **NAME "long"
%typemap(jstype) TYPE **NAME "TYPE"
%typemap(in) 	 TYPE **NAME (TYPE *temp = NULL) { temp = (TYPE*)$input; $1 = &temp; }
%typemap(argout) TYPE **NAME "$input = (jlong)*$1;" 
%typemap(directorin,descriptor="L$packagepath/TYPE;") TYPE **NAME "$input = (jlong)*$1;"
%typemap(directorargout) TYPE **NAME " *$1 = (TYPE*)$input; "
%typemap(javain) TYPE **NAME "TYPE.getCPtr($javainput)"
%typemap(javadirectorin) TYPE **NAME "($jniinput == 0) ? null : new TYPE($jniinput, false)"
%typemap(javadirectorout) TYPE **NAME "TYPE.getCPtr($javacall)"
%enddef


/* 
 * Generate setter/getter of a struct member typed of 'array of enum' with variable length.
 * Sample usage, given enum array member in this struct:
 * --
 *   struct pjsip_tls_setting {
 *     ..
 *     unsigned ciphers_num;
 *     pj_ssl_cipher *ciphers;
 *     ..
 *   };
 * --
 * apply:
 *   MY_JAVA_MEMBER_ARRAY_OF_ENUM(pjsip_tls_setting, pj_ssl_cipher, ciphers, ciphers_num)
 */
%define MY_JAVA_MEMBER_ARRAY_OF_ENUM(CLASS_NAME, TYPE, NAME, COUNT_NAME)

%apply ARRAYSOFENUMS[] { TYPE* };
%typemap(out) TYPE* %{ $result = SWIG_JavaArrayOutInt(jenv, (int*)$1, arg1->COUNT_NAME); %}
%ignore CLASS_NAME::NAME;
%ignore CLASS_NAME::COUNT_NAME;
%extend CLASS_NAME {
    %rename ("%(lowercamelcase)s") set_##NAME;
    %rename ("%(lowercamelcase)s") get_##NAME;
    void set_##NAME(TYPE *NAME, int num) {
        int i;
	if ($self->NAME) free($self->NAME);
	$self->NAME = (TYPE*)calloc(num, sizeof(TYPE));
	for (i=0; i<num; ++i) $self->NAME[i] = NAME[i];
	$self->COUNT_NAME = num;
    }
    TYPE* get_##NAME() {
	return $self->NAME;
    }
    ~CLASS_NAME() {
	if ($self->NAME) free($self->NAME);
    }
};

%enddef


/* 
 * Handle getter of a struct member with type of 'array of pointer to struct/class'.
 * Currently this is applicable for read-only member.
 */
%define MY_JAVA_MEMBER_ARRAY_OF_POINTER(CLASS_NAME, TYPE, NAME, COUNT_NAME)

%ignore CLASS_NAME::COUNT_NAME;
%immutable CLASS_NAME::NAME;
%typemap(jni) TYPE *[ANY] "jlongArray"
%typemap(jtype) TYPE *[ANY] "long[]"
%typemap(jstype) TYPE *[ANY] "TYPE[]"
%typemap(javain) TYPE *[ANY] "TYPE.cArrayUnwrap($javainput)"
%typemap(javaout) TYPE *[ANY] {return TYPE.cArrayWrap($jnicall, $owner);}

// somehow "$result" is not preprocessed in "typemap(ret)", use 'jresult' directly then
%typemap(ret) TYPE *[ANY] {
  jlong *arr;
  int i;
  jresult = JCALL1(NewLongArray, jenv, arg1->COUNT_NAME);
  if (!jresult) {
    return $null;
  }
  arr = JCALL2(GetLongArrayElements, jenv, jresult, 0);
  if (!arr) {
    return $null;
  }
  for (i=0; i<arg1->COUNT_NAME; i++) {
    arr[i] = 0;
    *($1_ltype)&arr[i] = $1[i];
  }
  JCALL3(ReleaseLongArrayElements, jenv, jresult, arr, 0);
}

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
