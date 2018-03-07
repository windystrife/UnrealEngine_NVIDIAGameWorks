#! /bin/sh
# script to set mcpp testsuite corresponding to the version of GCC 2 or 3, 4
# ./set_test.sh $CC $gcc_path $gcc_testsuite_dir $gcc_maj_ver $LN_S $cpp_call
#       $target_cc

CC=$1
gcc_path=`expr $2 : "\(.*\)/$CC"`
target_cc=$7
if test x$target_cc != x; then
    CC=$target_cc
fi
gcc_testsuite_dir=$3
gcc_maj_ver=$4
if test $gcc_maj_ver = 4; then
    gcc_maj_ver=3;
fi
LN_S=$5
cpp_name=`echo $6 | sed 's,.*/,,'`
cpp_path=`echo $6 | sed "s,/$cpp_name,,"`

echo "  cd $gcc_testsuite_dir/gcc.dg/cpp-test/test-t"
cd "$gcc_testsuite_dir/gcc.dg/cpp-test/test-t"
for i in *_run.c
do
    rm -f $i
    echo "  $LN_S $i.gcc$gcc_maj_ver $i"
    $LN_S $i.gcc$gcc_maj_ver $i
done

echo "  cd $cpp_path"
cd "$cpp_path"
echo "  appending '-23j' options to mcpp invocation"
for i in mcpp*.sh
do
    cat $i | sed 's/mcpp/mcpp -23j/' > tmp
    mv -f tmp $i
    chmod a+x $i
done

if test $CC = gcc; then
    exit 0
fi

echo "  cd $gcc_path"
cd "$gcc_path"
if test -f "gcc"; then
    echo "  mv gcc gcc.save"
    mv gcc gcc.save
fi
echo "  $LN_S $CC gcc"
$LN_S $CC gcc

