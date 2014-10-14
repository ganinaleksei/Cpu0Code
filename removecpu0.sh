CURR_DIR=`pwd`
cd ..
rm -f *.cpp *.h *.td CMakeLists.txt LLVMBuild.txt
rm -rf AsmParser
rm -rf Disassembler
rm -rf InstPrinter
rm -rf MCTargetDesc
rm -rf TargetInfo
cd ${CURR_DIR}
