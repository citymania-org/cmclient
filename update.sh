version=$1
dir=`mktemp -d`
curdir=`pwd`
echo $dir
cd $dir
wget https://proxy.binaries.openttd.org/openttd-releases/$version/openttd-$version-source.tar.xz
tar xf openttd-$version-source.tar.xz
ttdir=$dir/openttd-$version
cp -rT $ttdir $curdir
cd $ttdir
find . -type f > $dir/ttd_files
mkdir build
cd build
cmake ..
make -j
# cd objs/release
# make $ttdir/src/rev.cpp
cd $curdir
cp $ttdir/build/generated/rev.cpp src/rev.cpp.in
cat $dir/ttd_files | xargs git add
rm -rf $dir
