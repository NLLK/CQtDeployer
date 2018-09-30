#!/bin/bash

declare -a QTLIBS

BASE_DIR=$(dirname "$(readlink -f "$0")")
QTLIBS=( libQt5Sql.a libQt5Xml.a libQt5Core.a libQt5Test.a libQt5Network.a libQt5Bootstrap.a libQt5Concurrent.a)
echo "base dir $BASE_DIR"

git submodule update --init --recursive

make clean
find $BASE_DIR -type f -name 'Makefile' -exec rm {} \;
cd $BASE_DIR/qtBase

for var in "${QTLIBS[@]}"
do
        if [ -e "$BASE_DIR/staticQt/lib/$var" ]
	then
	    echo "$var - ok"
	else
	    echo "$var - not exits!. rebuild qt ..."
            rm -rdf $BASE_DIR/staticQt
            git clean -xdf    
            ./configure -confirm-license -prefix $BASE_DIR/staticQt -release -optimize-size -static -no-opengl -no-openssl -opensource -nomake tests -nomake examples -no-gui -no-widgets -no-dbus -no-accessibility    
            make install -j$(nproc)
	    break
	fi
done
	
cd ..
rm -rdf $BASE_DIR/build

export PATH=$PATH:$BASE_DIR/staticQt
$BASE_DIR/staticQt/bin/qmake QMAKE_LFLAGS+="-static -static-libgcc -static-libstdc++" CQtDeployer.pro

make -j$(nproc)

strip build/*
