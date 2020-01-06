version=$1
dir=`mktemp -d`
curdir=`pwd`
echo $dir
cd $dir
wget https://proxy.binaries.openttd.org/openttd-releases/$version/openttd-$version-source.tar.xz
#cp ~/Downloads/openttd-$version-source.tar.xz .
tar xf openttd-$version-source.tar.xz
ttdir=$dir/openttd-$version
cp -r $ttdir/* $curdir
cd $ttdir
find . -type f > $dir/ttd_files
./configure
cd objs/release
make $ttdir/src/rev.cpp
cd $curdir
cp $ttdir/src/rev.cpp src
cat $dir/ttd_files | xargs hg add
rm -rf $dir
