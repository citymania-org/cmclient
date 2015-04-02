TMP="/tmp/nova_release"
WIN="/var/run/media/pavels/F8040EC2040E843A/novattd"
rm -rf $TMP
echo $TMP
mkdir $TMP
unzip -d $TMP release.zip
cp -r $WIN/bin/lang $TMP
cp nova_changelog.txt $TMP/changelog.txt
rm $TMP/nova*.diff
cp nova150.diff $TMP
DIR=`pwd`
pushd $TMP
cp $WIN/bin/openttd.exe openttd.exe
rm $DIR/novapolis_client.zip
zip -9 -r $DIR/novapolis_client.zip *
cp $WIN/bin/openttd64.exe openttd.exe
rm $DIR/novapolis_client_64.zip
zip -9 -r $DIR/novapolis_client_64.zip *
popd