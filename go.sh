clear 

cd ..

make clean
make library
make library_install

cp agios.conf /tmp/agios.conf

cd Test

make clean
make all
