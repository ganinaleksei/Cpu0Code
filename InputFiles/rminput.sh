PWD=`pwd`
pushd ${PWD}
rm -rf a.out dlconfig cpu0.hex *~ libfoobar.cpu0.so
cd ../cpu0_verilog
rm -rf dlconfig cpu0.hex *~
popd

