version=$1
dir=`mktemp -d`
curdir=`pwd`
echo $dir
cd $dir
wget https://cdn.openttd.org/openttd-releases/$version/openttd-$version-source.tar.xz
tar xf openttd-$version-source.tar.xz
ttdir=$dir/openttd-$version
cp -rT $ttdir $curdir
cd $ttdir
find . -type f > $dir/ttd_files
find * > $dir/ttd_all
mkdir build
cd build
cmake ..
make -j 4
# cd objs/release
# make $ttdir/src/rev.cpp
cd $curdir
cp $ttdir/build/generated/rev.cpp src/rev.cpp.in
cat $dir/ttd_files | xargs git add
for i in $(find * -not -path "build*" -not -path "update.sh"); do
    if ! grep -qxFe "$i" $dir/ttd_all; then
        echo "Deleting: $i"
        # the next line is commented out.  Test it.  Then uncomment to removed the files
        git rm -f "$i"
    fi
done
# rm -rf $dir
