#!/bin/bash

c=$(id | grep 'yuh')
if [ -z "$c" ]; then
    echo 'Run: su yuh'
    exit
fi

mvn -Dmaven.test.skip=true
java -jar -Djava.library.path=microemulator/target microemulator/target/microemulator-2.0.4.jar -jar test-j2me.jar --resizableDevice 240 320