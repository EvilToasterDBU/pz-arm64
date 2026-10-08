#!/bin/bash
# usage: runlt.sh <TestClass> [libdir]
I=/run/media/armada/RP6-SD/steamapps/common/ProjectZomboid/projectzomboid
N=$HOME/pz-native
JAVA=$HOME/Games/PrismLauncher/java/java-runtime-gamma-snapshot/bin/java
cd $I
CP=".:$(ls $I/*.jar | tr '\n' ':')/tmp/pzprobe"
unset DISPLAY WAYLAND_DISPLAY
export LD_LIBRARY_PATH=$N/bridge
export LD_PRELOAD=$(dirname $(dirname $JAVA))/lib/libjsig.so
export BOX64_LD_LIBRARY_PATH=$N/x86:$I:$I/linux64:/usr/share/guestos/fex-mesa/usr/lib:/usr/share/guestos/fex-mesa/lib64
export BOX64_LOG=${BOX64_LOG:-1} BOX64_DYNAREC_STRONGMEM=1 PZB_DEBUG=1
exec timeout -k 5 120 $JAVA -Djava.awt.headless=true -Djava.library.path=${2:-$N/libs-real} -Xmx512m -cp "$CP" "$1"
