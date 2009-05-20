#!/bin/bash -x
cd $(dirname $0)/tk/macosx &&
xcodebuild -project Tk.xcode -target tktest -configuration DebugLeaks clean &&
scan-build -k -V xcodebuild -project Tk.xcode -target tktest -configuration DebugLeaks
