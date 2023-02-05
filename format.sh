#!/bin/sh

#
# use clang-format to format code
#

# clang-format version must >= 13
MIN_VERSION=13

list_files() {
    find $1 \
        -name "*.h" -o -name "*.c" \
        -o -name "*.hpp" -o -name "*.hh" -o -name "*.hxx" \
        -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \
        -o -name "*.m"  \
        -o -name "*.java" \
        -o -name "*.cs"
}

format_all() {
    DIRS="pjlib pjlib-util pjmedia pjnath pjsip pjsip-apps tests"
    for d in $DIRS; do
        clang-format --style=file -i $(list_files $d)
    done
}

check_clang_format() {
    if [ -z $(command -v clang-format) ];then
        echo "can't find command clang-format !!" >&2
        exit 1
    fi

    v=$(clang-format --version |grep -Eo "version [0-9]+" |grep -Eo "[0-9]+")
    if [ $v -lt $MIN_VERSION ];then
        echo $(clang-format --version | head -2) >&2
        echo "version must >= $MIN_VERSION !!" >&2
        exit 2
    fi
}

# main
check_clang_format

case "$1" in
    pj*|tests)
        clang-format --style=file -i $(list_files $1)
        ;;
    all)
        format_all
        ;;
    *)
        echo $"Usage: $0 {pjlib|pjlib-util|pjmedia|pjnath|pjsip|pjsip-apps|tests|all}"
        exit 2
esac

