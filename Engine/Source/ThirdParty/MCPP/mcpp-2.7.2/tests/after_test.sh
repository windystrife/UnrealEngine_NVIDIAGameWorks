#!/bin/sh
## ./after_test.sh $CC $gcc_path $cpp_call $target_cc

CC=$1
gcc_path=`expr $2 : "\(.*\)/${CC}\$"`
target_cc=$4
if test x$target_cc != x; then
    CC=$target_cc
fi
cpp_name=`echo $3 | sed 's,.*/,,'`
cpp_path=`echo $3 | sed "s,/$cpp_name,,"`

echo "  cd $cpp_path"
cd "$cpp_path"
echo "  removing '-23j' options from mcpp invocation"
for i in mcpp*.sh
do
    cat $i | sed 's/mcpp -23j/mcpp/' > tmp
    mv -f tmp $i
    chmod a+x $i
done

if test $CC = gcc; then
    exit 0
fi

echo "  cd $gcc_path"
cd "$gcc_path"
echo "  rm gcc"
rm gcc
if test -f "gcc.save"; then
    echo "  mv gcc.save gcc"
    mv gcc.save gcc
fi
