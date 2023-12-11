
#include "KeyPointsCollector.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Common.h"

KeyPointsCollector::KeyPointsCollector(const std::string &filename, bool debug)
    : filename(std::move(filename)), debug(debug) {

  std::ifstream file(filename);
  if (file.good()) {
    file.close();

   
    std::stringstream formatCommand;
    formatCommand << "clang-format -i --style=file:file_format_style "
                  << filename;
    system(formatCommand.str().c_str());

    removeIncludeDirectives();


    translationUnit = clang_createTranslationUnitFromSourceFile(
        KPCIndex, filename.c_str(), 0, nullptr, 0, nullptr);

    reInsertIncludeDirectives();


    if (translationUnit == nullptr) {
      std::cerr
          << "There was an error parsing the translation unit! Exiting...\n";
      exit(EXIT_FAILURE);
    }
    std::cout << "Translation unit for file: " << filename
              << " successfully parsed.\n";


    rootCursor = clang_getTranslationUnitCursor(translationUnit);
    cxFile = clang_getFile(translationUnit, filename.c_str());
    branchCount = 0;
 
  } else {
    std::cerr << "File with name: " << filename
              << ", does not exist! Exiting...\n";
    exit(EXIT_FAILURE);
  }
}

KeyPointsCollector::~KeyPointsCollector() {
  clang_disposeTranslationUnit(translationUnit);
}

void KeyPointsCollector::removeIncludeDirectives() {
  std::ifstream file(filename);
  std::ofstream tempFile("temp.c");
  std::string currentLine;
  const std::string includeStr("#include");
  unsigned lineNum = 1;

  if (file.good() && tempFile.good()) {
    while (getline(file, currentLine)) {
      if (currentLine.find(includeStr) == 0) {
        addIncludeDirective(lineNum++, currentLine);
        continue;
      }
      lineNum++;
      tempFile << currentLine << '\n';
    }
  }
  std::remove(filename.c_str());
  std::rename("temp.c", filename.c_str());
}

void KeyPointsCollector::reInsertIncludeDirectives() {
  std::ifstream file(filename);
  std::ofstream tempFile("temp.c");
  std::string currentLine;
  const std::string includeStr("#include");
  unsigned lineNum = 1;

  if (file.good() && tempFile.good()) {
    while (getline(file, currentLine)) {
      if (MAP_FIND(includeDirectives, lineNum)) {
        while(MAP_FIND(includeDirectives, lineNum)) {
          tempFile << includeDirectives[lineNum++] << '\n';
        }
        tempFile << currentLine << '\n';
      } else {
        lineNum++;
        tempFile << currentLine << '\n';
      }
    }
  }

  std::remove(filename.c_str());
  std::rename("temp.c", filename.c_str());
}

bool KeyPointsCollector::isBranchPointOrCallExpr(const CXCursorKind K) {
  switch (K) {
  case CXCursor_IfStmt:
  case CXCursor_ForStmt:
  case CXCursor_DoStmt:
  case CXCursor_WhileStmt:
  case CXCursor_SwitchStmt:
  case CXCursor_CallExpr:
    return true;
  default:
    return false;
  }
}

bool KeyPointsCollector::isFunctionPtr(const CXCursor C) {
  CXType cursorType = clang_getCursorType(C);
  if (cursorType.kind == CXType_Pointer) {
    CXType ptrType = clang_getPointeeType(cursorType);
    return ptrType.kind == CXType_FunctionProto;
  }
  return false;
}

bool KeyPointsCollector::checkChildAgainstStackTop(CXCursor child) {
  unsigned childLineNum;
  unsigned childColNum;
  BranchPointInfo *currBranch = getCurrentBranch();
  CXSourceLocation childLoc = clang_getCursorLocation(child);
  clang_getSpellingLocation(childLoc, getCXFile(), &childLineNum, &childColNum,
                            nullptr);

  if (inCurrentFunction(childLineNum)) {
    if (childLineNum > currBranch->compoundEndLineNum ||
        (childLineNum == currBranch->compoundEndLineNum &&
         childColNum > currBranch->compoundEndColumnNum)) {
      getCurrentBranch()->addTarget(childLineNum + getNumIncludeDirectives());
      if (debug) {
        printFoundTargetPoint();
      }
      return true;
    } else {
      return false;
    }
  }
  return false;
}

CXChildVisitResult KeyPointsCollector::VisitorFunctionCore(CXCursor current,
                                                           CXCursor parent,
                                                           CXClientData kpc) {
  KeyPointsCollector *instance = static_cast<KeyPointsCollector *>(kpc);
  const CXCursorKind currKind = clang_getCursorKind(current);
  const CXCursorKind parrKind = clang_getCursorKind(parent);

  
  if (currKind == CXCursor_CallExpr) {
    clang_visitChildren(parent, &KeyPointsCollector::VisitCallExpr, kpc);
    return CXChildVisit_Continue;
  }

 
  if (instance->isBranchPointOrCallExpr(parrKind) &&
      currKind == CXCursor_CompoundStmt) {
    
    instance->addCursor(parent);
    instance->pushNewBranchPoint();
    CXSourceLocation loc = clang_getCursorLocation(parent);
    clang_getSpellingLocation(loc, instance->getCXFile(),
                              instance->getCurrentBranch()->getBranchPointOut(),
                              nullptr, nullptr);
    instance->getCurrentBranch()->branchPoint +=
        instance->getNumIncludeDirectives();

    if (instance->debug) {
      instance->printFoundBranchPoint(parrKind);
    }

    clang_visitChildren(current, &KeyPointsCollector::VisitCompoundStmt, kpc);

    BranchPointInfo *currBranch = instance->getCurrentBranch();
    CXSourceLocation parentEnd =
        clang_getRangeEnd(clang_getCursorExtent(parent));
    clang_getSpellingLocation(parentEnd, instance->getCXFile(),
                              &(currBranch->compoundEndLineNum), nullptr,
                              nullptr);
  }

  if (instance->compoundStmtFoundYet() &&
      instance->getCurrentBranch()->compoundEndLineNum != 0 &&
      instance->checkChildAgainstStackTop(current)) {
    instance->addCompletedBranch();
  }

  if (currKind == CXCursor_FunctionDecl) {
    clang_visitChildren(current, &KeyPointsCollector::VisitFuncDecl, kpc);
  }

  if (currKind == CXCursor_VarDecl || currKind == CXCursor_ParmDecl) {
    clang_visitChildren(parent, &KeyPointsCollector::VisitVarOrParamDecl, kpc);
  }

  return CXChildVisit_Recurse;
}

CXChildVisitResult KeyPointsCollector::VisitCompoundStmt(CXCursor current,
                                                         CXCursor parent,
                                                         CXClientData kpc) {
  KeyPointsCollector *instance = static_cast<KeyPointsCollector *>(kpc);
  const CXCursorKind currKind = clang_getCursorKind(current);
  const CXCursorKind parrKind = clang_getCursorKind(parent);
  if (parrKind != CXCursor_CompoundStmt) {
    std::cerr << "Compound statement visitor called when cursor is not "
                 "compound stmt!\n";
    exit(EXIT_FAILURE);
  }
  unsigned targetLineNumber;
  CXSourceLocation loc = clang_getCursorLocation(current);
  clang_getSpellingLocation(loc, instance->getCXFile(), &targetLineNumber,
                            nullptr, nullptr);

  instance->getCurrentBranch()->addTarget(targetLineNumber +
                                          instance->getNumIncludeDirectives());
  if (instance->debug) {
    instance->printFoundTargetPoint();
  }
  return CXChildVisit_Continue;
}

CXChildVisitResult KeyPointsCollector::VisitCallExpr(CXCursor current,
                                                     CXCursor parent,
                                                     CXClientData kpc) {
  KeyPointsCollector *instance = static_cast<KeyPointsCollector *>(kpc);

  CXSourceLocation callExprLoc = clang_getCursorLocation(current);
  CXToken *calleeNameTok = clang_getToken(instance->getTU(), callExprLoc);
  CXString calleeNameStr =
      clang_getTokenSpelling(instance->getTU(), *calleeNameTok);
  std::string calleeName(clang_getCString(calleeNameStr));

  if (MAP_FIND(instance->funcDeclsString, calleeName)) {
    unsigned callLocLine;
    clang_getSpellingLocation(callExprLoc, instance->getCXFile(), &callLocLine,
                              nullptr, nullptr);
    instance->addCall(callLocLine + instance->getNumIncludeDirectives(),
                      calleeName);

    if (instance->getFunctionByName(calleeName)->isInBody(callLocLine)) {
      instance->getFunctionByName(calleeName)->setRecursive();
    }
    clang_disposeTokens(instance->getTU(), calleeNameTok, 1);
    clang_disposeString(calleeNameStr);

    return CXChildVisit_Break;
  } else if (MAP_FIND(instance->funcPtrs, calleeName)) {
    unsigned callLocLine;
    clang_getSpellingLocation(callExprLoc, instance->getCXFile(), &callLocLine,
                              nullptr, nullptr);
    instance->addCall(callLocLine + instance->getNumIncludeDirectives(),
                      instance->funcPtrs[calleeName]);
    clang_disposeTokens(instance->getTU(), calleeNameTok, 1);
    clang_disposeString(calleeNameStr);
    return CXChildVisit_Break;
  }
  clang_disposeString(calleeNameStr);
  clang_disposeTokens(instance->getTU(), calleeNameTok, 1);

  return CXChildVisit_Recurse;
}

CXChildVisitResult KeyPointsCollector::VisitFuncPtr(CXCursor current,
                                                    CXCursor parent,
                                                    CXClientData kpc) {
  KeyPointsCollector *instance = static_cast<KeyPointsCollector *>(kpc);

  CXSourceLocation funcPtrLoc = clang_getCursorLocation(parent);
  CXToken *funcPtrTok = clang_getToken(instance->getTU(), funcPtrLoc);
  CXString funcPtrStr = clang_getTokenSpelling(instance->getTU(), *funcPtrTok);
  std::string funcPtrName(clang_getCString(funcPtrStr));

  if (!(MAP_FIND(instance->funcPtrs, funcPtrName)) &&
      instance->currFuncPtrId.empty()) {
    instance->currFuncPtrId = funcPtrName;
  }


  CXSourceLocation funcPteeLoc = clang_getCursorLocation(current);
  CXToken *funcPteeTok = clang_getToken(instance->getTU(), funcPteeLoc);
  CXString funcPteeStr =
      clang_getTokenSpelling(instance->getTU(), *funcPteeTok);
  std::string funcPteeName(clang_getCString(funcPteeStr));

  if (instance->getFunctionByName(funcPteeName) != nullptr) {
    instance->funcPtrs[instance->currFuncPtrId] = funcPteeName;
    instance->currFuncPtrId.clear();
    clang_disposeTokens(instance->getTU(), funcPtrTok, 1);
    clang_disposeTokens(instance->getTU(), funcPteeTok, 1);
    clang_disposeString(funcPtrStr);
    clang_disposeString(funcPteeStr);
    return CXChildVisit_Break;
  }

  clang_disposeString(funcPtrStr);
  clang_disposeString(funcPteeStr);
  clang_disposeTokens(instance->getTU(), funcPtrTok, 1);
  clang_disposeTokens(instance->getTU(), funcPteeTok, 1);

  return CXChildVisit_Recurse;
}

CXChildVisitResult KeyPointsCollector::VisitVarOrParamDecl(CXCursor current,
                                                           CXCursor parent,
                                                           CXClientData kpc) {
  KeyPointsCollector *instance = static_cast<KeyPointsCollector *>(kpc);

  unsigned varDeclLineNum;
  CXSourceLocation varDeclLoc = clang_getCursorLocation(current);
  clang_getSpellingLocation(varDeclLoc, instance->getCXFile(), &varDeclLineNum,
                            nullptr, nullptr);

  if (instance->isFunctionPtr(current)) {
    clang_visitChildren(current, &KeyPointsCollector::VisitFuncPtr, kpc);
    return CXChildVisit_Break;
  }

  CXToken *varDeclToken = clang_getToken(instance->getTU(), varDeclLoc);
  std::string varName =
      CXSTR(clang_getTokenSpelling(instance->getTU(), *varDeclToken));

  std::map<std::string, unsigned> varMap = instance->getVarDecls();

  if (varMap.find(varName) == varMap.end()) {
    if (instance->debug) {
      std::cout << "Found "
                << (current.kind == CXCursor_VarDecl ? "VarDecl" : "ParamDecl")
                << ": " << varName << " at line # " << varDeclLineNum << '\n';
    }
    instance->addVarDeclToMap(varName, varDeclLineNum +
                                           instance->getNumIncludeDirectives());
  }
  clang_disposeTokens(instance->getTU(), varDeclToken, 1);
  return CXChildVisit_Break;
}

CXChildVisitResult KeyPointsCollector::VisitFuncDecl(CXCursor current,
                                                     CXCursor parent,
                                                     CXClientData kpc) {
  KeyPointsCollector *instance = static_cast<KeyPointsCollector *>(kpc);

  if (clang_getCursorKind(parent) == CXCursor_FunctionDecl) {
    CXType funcReturnType = clang_getResultType(clang_getCursorType(parent));

    CXString funcReturnTypeSpelling = clang_getTypeSpelling(funcReturnType);
    unsigned begLineNum, endLineNum;
    CXSourceRange funcRange = clang_getCursorExtent(parent);
    CXSourceLocation funcBeg = clang_getRangeStart(funcRange);
    CXSourceLocation funcEnd = clang_getRangeEnd(funcRange);
    clang_getSpellingLocation(funcBeg, instance->getCXFile(), &begLineNum,
                              nullptr, nullptr);
    clang_getSpellingLocation(funcEnd, instance->getCXFile(), &endLineNum,
                              nullptr, nullptr);

    CXToken *funcDeclToken =
        clang_getToken(instance->getTU(), clang_getCursorLocation(parent));
    std::string funcName =
        CXSTR(clang_getTokenSpelling(instance->getTU(), *funcDeclToken));

    instance->addFuncDecl(std::make_shared<FunctionDeclInfo>(
        begLineNum + instance->getNumIncludeDirectives(),
        endLineNum + instance->getNumIncludeDirectives(), funcName,
        clang_getCString(funcReturnTypeSpelling)));
    instance->currentFunction = instance->getFunctionByName(funcName);
    if (instance->debug) {
      std::cout << "Found FunctionDecl: " << funcName << " of return type: "
                << clang_getCString(funcReturnTypeSpelling)
                << " on line #: " << begLineNum << '\n';
    }
    clang_disposeTokens(instance->getTU(), funcDeclToken, 1);
    clang_disposeString(funcReturnTypeSpelling);
  }

  return CXChildVisit_Break;
}

void KeyPointsCollector::collectCursors() {
  clang_visitChildren(rootCursor, this->VisitorFunctionCore, this);
  addBranchesToDictionary();
}

void KeyPointsCollector::printFoundBranchPoint(const CXCursorKind K) {
  std::cout << "Found branch point: " << CXSTR(clang_getCursorKindSpelling(K))
            << " at line#: " << getCurrentBranch()->branchPoint << '\n';
}

void KeyPointsCollector::printFoundTargetPoint() {
  BranchPointInfo *currentBranch = getCurrentBranch();
  std::cout << "Found target for line branch #: " << currentBranch->branchPoint
            << " at line#: " << currentBranch->targetLineNumbers.back() << '\n';
}

void KeyPointsCollector::printCursorKind(const CXCursorKind K) {
  std::cout << "Found cursor: " << CXSTR(clang_getCursorKindSpelling(K))
            << '\n';
}

void KeyPointsCollector::createDictionaryFile() {
  std::ofstream dictFile(std::string(OUT_DIR + filename + ".branch_dict"));
  dictFile << "Branch Dictionary for: " << filename << '\n';
  dictFile << "-----------------------" << std::string(filename.size(), '-')
           << '\n';

  const std::map<unsigned, std::map<unsigned, std::string>> &branchDict =
      getBranchDictionary();

  for (const std::pair<unsigned, std::map<unsigned, std::string>> &BP :
       branchDict) {
    for (const std::pair<unsigned, std::string> &targets : BP.second) {
      dictFile << targets.second << ": " << filename << ", " << BP.first << ", "
               << targets.first << '\n';
    }
  }

  dictFile.close();
}

void KeyPointsCollector::addCompletedBranch() {
  branchPoints.push_back(branchPointStack.top());
  branchPointStack.pop();
}

void KeyPointsCollector::addBranchesToDictionary() {
  for (std::vector<BranchPointInfo>::reverse_iterator branchPoint =
           branchPoints.rbegin();
       branchPoint != branchPoints.rend(); branchPoint++) {
    std::map<unsigned, std::string> targetsAndIds;
    for (const unsigned &target : branchPoint->targetLineNumbers) {
      targetsAndIds[target] = "br_" + std::to_string(++branchCount);
    }
    branchDictionary[branchPoint->branchPoint] = targetsAndIds;
  }
}

void KeyPointsCollector::transformProgram() {
  std::ifstream originalProgram(filename);
  std::ofstream modifiedProgram(MODIFIED_PROGAM_OUT);

  if (originalProgram.good() && modifiedProgram.good()) {
    modifiedProgram << TRANSFORM_HEADER;

    unsigned lineNum = 1;

    std::string currentLine;

    std::shared_ptr<FunctionDeclInfo> currentTransformFunction = nullptr;

    int branchCountCurrFunc;

    std::map<unsigned, std::shared_ptr<FunctionDeclInfo>> funcDecls =
        getFuncDecls();

    std::map<unsigned, std::string> funcCalls = getFuncCalls();

    std::map<unsigned, std::map<unsigned, std::string>> branchDict =
        getBranchDictionary();

    std::vector<unsigned> foundPoints;

    while (getline(originalProgram, currentLine)) {
      if (MAP_FIND(funcDecls, lineNum - 1)) {
        currentTransformFunction = funcDecls[lineNum - 1];

        if (currentTransformFunction->name.compare("main") &&
            currentTransformFunction->recursive &&
            currentTransformFunction->type != "void") {
          modifiedProgram << DECLARE_FUNC_PTR(currentTransformFunction);
        }

        foundPoints.clear();
        branchCountCurrFunc = 0;
        insertFunctionBranchPointDecls(
            modifiedProgram, currentTransformFunction, &branchCountCurrFunc);
      }

      if (currentTransformFunction != nullptr &&
          (lineNum - 1) == currentTransformFunction->endLoc &&
          currentTransformFunction->name.compare("main")) {
        modifiedProgram << DECLARE_FUNC_PTR(currentTransformFunction);
      }

      if (MAP_FIND(branchDict, lineNum - 1)) {
        modifiedProgram << SET_BRANCH(foundPoints.size());
        foundPoints.push_back(lineNum - 1);
      }


      std::vector<unsigned> foundPointsIdxCurrentLine;

      if (!foundPoints.empty()) {
        for (int idx = foundPoints.size() - 1; idx >= 0; --idx) {
          std::map<unsigned, std::string> targets =
              branchDict[foundPoints[idx]];

          if (MAP_FIND(targets, lineNum)) {
            foundPointsIdxCurrentLine.push_back(idx);
          }
        }
      }
      switch (foundPointsIdxCurrentLine.size()) {
      case 0:
        break;
    
      case 1: {
       
        if (foundPointsIdxCurrentLine[0] + 1 < branchCountCurrFunc) {
          modifiedProgram << "if (";
          for (int successive = foundPointsIdxCurrentLine[0] + 1;
               successive < branchCountCurrFunc; successive++) {
            modifiedProgram << "!BRANCH_" << successive;
            if (branchCountCurrFunc - successive > 1)
              modifiedProgram << " && ";
          }
          modifiedProgram
              << ") LOG(\""
              << branchDict[foundPoints[foundPointsIdxCurrentLine[0]]][lineNum]
              << "\");";
        }
        else {
          modifiedProgram
              << "LOG(\""
              << branchDict[foundPoints[foundPointsIdxCurrentLine[0]]][lineNum]
              << "\");";
        }
        break;
      }
      case 2: {
        modifiedProgram
            << "if (BRANCH_" << foundPointsIdxCurrentLine[0] << ") {LOG(\""
            << branchDict[foundPoints[foundPointsIdxCurrentLine[0]]][lineNum]
            << "\")} else {LOG(\""
            << branchDict[foundPoints[foundPointsIdxCurrentLine[1]]][lineNum]
            << "\")}";
        break;
      }
      default: {
        modifiedProgram
            << "if (BRANCH_" << foundPointsIdxCurrentLine[0] << ") {LOG(\""
            << branchDict[foundPoints[foundPointsIdxCurrentLine[0]]][lineNum]
            << "\")}";

        for (int successive = 1;
             successive < foundPointsIdxCurrentLine.size() - 1; successive++) {
          modifiedProgram
              << " else if (BRANCH_" << foundPointsIdxCurrentLine[successive]
              << ") {LOG(\""
              << branchDict[foundPoints[foundPointsIdxCurrentLine[successive]]]
                           [lineNum]
              << "\")}";
        }

        // Insert final else for the last branch point.
        modifiedProgram
            << "else {LOG(\""
            << branchDict[foundPoints[foundPointsIdxCurrentLine
                                          [foundPointsIdxCurrentLine.size() -
                                           1]]][lineNum]
            << "\")}";

      } break;
      }

      if (MAP_FIND(funcCalls, lineNum)) {
        modifiedProgram << "LOG_PTR(" << funcCalls[lineNum] << "_PTR"
                        << ");\n";
      }


      modifiedProgram << WRITE_LINE(currentLine);
      lineNum++;
    }

    originalProgram.close();
    modifiedProgram.close();

  } else {
    std::cerr << "Error opening program files for transformation!\n";
    exit(EXIT_FAILURE);
  }
}

void KeyPointsCollector::insertFunctionBranchPointDecls(
    std::ofstream &program, std::shared_ptr<FunctionDeclInfo> function,
    int *branchCount) {
  for (int lineNum = function->defLoc; lineNum < function->endLoc; lineNum++) {
    if (MAP_FIND(getBranchDictionary(), lineNum)) {
      program << DECLARE_BRANCH((*branchCount)++);
    }
  }
  program << '\n';
}

void KeyPointsCollector::compileModified() {
#if defined(__clang__)
  std::string c_compiler("clang");
#elif defined(__GNUC__)
  std::string c_compiler("gcc");
#endif
  if (c_compiler.empty()) {
    c_compiler = std::getenv("CC");
    if (c_compiler.empty()) {
      std::cerr << "No viable C compiler found on system!\n";
      exit(EXIT_FAILURE);
    }
  }
  std::cout << "C compiler is: " << c_compiler << '\n';

  if (!static_cast<bool>(std::ifstream(MODIFIED_PROGAM_OUT).good())) {
    std::cerr << "Transformed program has not been created yet!\n";
    exit(EXIT_FAILURE);
  }

  std::stringstream compilationCommand;
  compilationCommand << c_compiler << " -w -O0 " << MODIFIED_PROGAM_OUT
                     << " -o " << EXE_OUT;

  bool compiled = static_cast<bool>(system(compilationCommand.str().c_str()));

  if (compiled == EXIT_SUCCESS) {
    std::cout << "Compilation Successful" << '\n';
  } else {
    std::cerr << "There was an error with compilation, exiting!\n";
    exit(EXIT_FAILURE);
  }
}

void KeyPointsCollector::invokeValgrind() {
#if defined(__clang__)
  std::string c_compiler("clang");
#elif defined(__GNUC__)
  std::string c_compiler("gcc");
#endif
  if (c_compiler.empty()) {
    c_compiler = std::getenv("CC");
    if (c_compiler.empty()) {
      std::cerr << "No viable C compiler found on system!\n";
      exit(EXIT_FAILURE);
    }
  }

  if (!static_cast<bool>(std::ifstream(filename).good())) {
    std::cerr << "No program to compile!\n";
    exit(EXIT_FAILURE);
  }

  std::stringstream shellCommandStream;
  shellCommandStream << c_compiler << " -O0 " << filename << " -o "
                     << ORIGINAL_EXE_OUT;

  bool compiled = static_cast<bool>(system(shellCommandStream.str().c_str()));

  if (compiled == EXIT_SUCCESS) {
    std::cout << "Compilation Successful" << '\n';
  } else {
    std::cerr << "There was an error with compilation, exiting!\n";
    exit(EXIT_FAILURE);
  }

  const std::string valgrindLogFile(OUT_DIR + filename + ".VALGRIND_OUT");
  shellCommandStream.str("");
  shellCommandStream.clear();
  shellCommandStream << "valgrind --tool=callgrind --dump-instr=yes --log-file="
                     << valgrindLogFile << " " << ORIGINAL_EXE_OUT;

  bool valgrind = static_cast<bool>(system(shellCommandStream.str().c_str()));

  if (valgrind == EXIT_SUCCESS) {
    std::cout << "Valgrind invoked successfully\n";
    system("rm -r callgrind*");
    shellCommandStream.str("");
    shellCommandStream.clear();
    shellCommandStream << "python3 " << VALGRIND_PARSER << " "
                       << valgrindLogFile;
    system(shellCommandStream.str().c_str());
  }
}

void KeyPointsCollector::executeToolchain() {
  collectCursors();
  createDictionaryFile();
  transformProgram();
  compileModified();
  std::cout << "\nToolchain was successful, the branch dicitonary, modified "
               "file, and executable have been written to the "
            << OUT_DIR << " directory \n";

  char decision;
  std::cout << "\nWould you like to invoke Valgrind? (y/n) ";
  std::cin >> decision;
  if (decision == 'y') {
    invokeValgrind();
  }

  std::cout << "\nWould you like to out put the branch pointer trace for the "
               "program? (y/n) ";
  std::cin >> decision;
  if (decision == 'y') {
    system(EXE_OUT.c_str());
  }
}

std::string KeyPointsCollector::getBPTrace() {
  collectCursors();
  transformProgram();
  compileModified();
  std::vector<char> buffer(128);
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(EXE_OUT.c_str(), "r"),
                                                pclose);
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}
