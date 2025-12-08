#!/bin/sh
msg=$(git log -1 --pretty=%s)
case "$msg" in
  "Update: Translations from eints"*)
    echo "⏩ Skipping build for translation commit"
    ;;
  *)
    cd build && ninja -j 20 openttd
    ;;
esac

# cd build && make -j 10
