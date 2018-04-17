TMP="/tmp/cmclient"
WIN="/media/pavels/F8040EC2040E843A/cmclient"
VER="1.8.0"
CMVER="1.8.0"
TMP_SRC="$TMP/openttd-$VER"
DIR=`pwd`
SRC_RELEASE_FNAME="$DIR/citymania-client-$CMVER-source.zip"
RELEASE_FNAME="$DIR/citymania-client-$CMVER-win32.zip"
RELEASE64_FNAME="$DIR/citymania-client-$CMVER-win64.zip"
RELEASE_DIFF="$DIR/citymania-client-$CMVER.diff"
EXE32="$WIN/objs/Win32/Release/openttd.exe"
EXE64="$WIN/objs/x64/Release/openttd.exe"
rm -f $SRC_RELEASE_FNAME
rm -f $RELEASE_FNAME
rm -f $RELEASE64_FNAME
rm -rf $TMP

set -e

cp $EXE32 $DIR/openttd.exe
cp $EXE64 $DIR/openttd64.exe
mkdir $TMP
hg diff -r openttd -B --nodates -X src/rev.cpp -X release_files -X make_diff.sh -X release.sh -X check_diff.sh -X novattd.sublime-project -X .hgignore -X src/rev.cpp.in -X Makefile.src.in -X build-number.txt > $RELEASE_DIFF
tar xf ~/Downloads/openttd-$VER-source.tar.* -C $TMP
patch -p1 -d $TMP_SRC < $RELEASE_DIFF
pushd $TMP_SRC
zip -9 -r $SRC_RELEASE_FNAME .
./configure
make lang
popd
unzip ~/Downloads/openttd-$VER-windows-win32.zip -d $TMP/release
mkdir $TMP/release/data
cp -r $TMP_SRC/bin/lang $TMP/release/
cp -r $DIR/release_files/* $TMP/release/
cp $DIR/cm_changelog.txt $TMP/release/citymania_changelog.txt
pushd $TMP/release
# cp $EXE32 openttd.exe
# zip -9 -r $RELEASE_FNAME .
cp $EXE64 openttd.exe
zip -9 -r $RELEASE64_FNAME .
popd
