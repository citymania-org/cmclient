TMP="/tmp/nova_release"
rm -rf $TMP
echo $TMP
mkdir $TMP
unzip -d $TMP release.zip
cp -r bin/lang $TMP
cp nova_changelog.txt $TMP/changelog.txt
cp -r bin/lang $TMP
rm $TMP/nova*.diff
cp nova*.diff $TMP
