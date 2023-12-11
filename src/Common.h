
#include <clang-c/Index.h>

#define CXSTR(X) clang_getCString(X)
#define QKDBG(OUT) std::cout << OUT << '\n';
#define QKCURSDBG(OUT) std::cout << CXSTR(clang_getCursorKindSpelling(OUT)) << std::endl;

#define OUT_DIR "out/"
#define EXE_OUT std::string(OUT_DIR + filename + ".modified.out")
#define MODIFIED_PROGAM_OUT std::string(OUT_DIR + filename + ".modified.c")
#define ORIGINAL_EXE_OUT std::string(OUT_DIR + filename + ".original.out")

#define VALGRIND_PARSER "valgrind_parser.py"


#define MAP_FIND(MAP, KEY) MAP.find(KEY) != MAP.end()

#define TRANSFORM_HEADER                                                       \
  "#include <stdio.h>\n#define LOG(BP) printf(\"%s\\n\", BP);\n#define "       \
  "LOG_PTR(PTR) printf(\"func_%p\\n\", PTR);\n"

#define DECLARE_BRANCH(BRANCH) "int BRANCH_" << BRANCH << " = 0;\n"
#define SET_BRANCH(BRANCH) "BRANCH_" << BRANCH << " = 1;\n"
#define WRITE_LINE(LINE) LINE << '\n';

#define DECLARE_FUNC_PTR(FUNC)                                                 \
  FUNC->type << " *" << FUNC->name << "_PTR = &" << FUNC->name << ";\n"
