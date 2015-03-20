DIFF=$1
ORIG_PATH=$2
TMP=/tmp/openttd_original
rm -rf $TMP
cp -r $ORIG_PATH $TMP
mkdir $TMP/bin
cp -r bin/data $TMP/bin
patch -p1 -d $TMP < $DIFF
pushd $TMP
./configure
make clean
make
popd
