git fetch upstream
git checkout upstream/master
git rebase -i master --exec "sh rebase-check.sh"
git checkout master
# git merge <detached head>
# possibly git reset --hard upstream/master
# check git merge-base master upstream/master
