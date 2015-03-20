TMP="/tmp/nova_release"
rm -rf $TMP
echo $TMP
mkdir $TMP
unzip -d $TMP release.zip
cp -r bin/lang $TMP
cp nova_changelog.txt $TMP/changelog.txt
rm $TMP/nova*.diff
cp nova*.diff $TMP
DIR=`pwd`
pushd $TMP
cp $DIR/bin/openttd.exe openttd.exe
rm $DIR/novapolis_client.zip
zip -9 -r $DIR/novapolis_client.zip *
cp $DIR/bin/openttd64.exe openttd.exe
rm $DIR/novapolis_client_64.zip
zip -9 -r $DIR/novapolis_client_64.zip *
popd