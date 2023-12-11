
#ifndef KEY_POINTS_COLLECTOR__H
#define KEY_POINTS_COLLECTOR__H

#include "Common.h"
#include <clang-c/Index.h>

#include <iostream>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>

class KeyPointsCollector {

  const std::string filename;

  
  CXFile cxFile;

  
  std::vector<CXCursor> cursorObjs;

  void addCursor(CXCursor const &C) { cursorObjs.push_back(C); }

  inline static CXIndex KPCIndex = clang_createIndex(0, 0);

  CXTranslationUnit translationUnit;

  CXCursor rootCursor;

  bool debug;

  std::map<unsigned, std::string> includeDirectives;

  void addIncludeDirective(unsigned lineNum, std::string includeDirective) {
    includeDirectives[lineNum] = includeDirective;
  }

  void removeIncludeDirectives();

  void reInsertIncludeDirectives();

  static CXChildVisitResult
  VisitorFunctionCore(CXCursor current, CXCursor parent, CXClientData kpc);

  
  static CXChildVisitResult VisitCompoundStmt(CXCursor current, CXCursor parent,
                                              CXClientData kpc);

  static CXChildVisitResult VisitCallExpr(CXCursor current, CXCursor parent,
                                          CXClientData kpc);

  static CXChildVisitResult VisitFuncDecl(CXCursor current, CXCursor parent,
                                          CXClientData kpc);

  static CXChildVisitResult
  VisitVarOrParamDecl(CXCursor current, CXCursor parent, CXClientData kpc);

  static CXChildVisitResult VisitFuncPtr(CXCursor current, CXCursor parent,
                                         CXClientData kpc);


  std::map<std::string, std::string> funcPtrs;

  std::string currFuncPtrId;

  std::string isFunctionPtr(const std::string &id) {
    if (MAP_FIND(funcPtrs, id)) {
      return funcPtrs[id];
    }
    return nullptr;
  }

  
  void addFuncPtr(const std::string &id, const std::string &func) {
    funcPtrs[id] = func;
  }

  struct FunctionDeclInfo {
    unsigned defLoc;
    unsigned endLoc;
    const std::string name;
    const std::string type;
    bool recursive;

    FunctionDeclInfo(unsigned defLoc, unsigned endLoc, const std::string &name,
                     const std::string &type)
        : defLoc(defLoc), endLoc(endLoc), name(std::move(name)),
          type(std::move(type)) {}

    void setRecursive() { recursive = true; }

    bool isInBody(unsigned lineNum) {
      return lineNum >= defLoc && lineNum <= endLoc;
    }
  };

  void addFuncDecl(std::shared_ptr<FunctionDeclInfo> decl) {
    funcDecls[decl->defLoc] = decl;
    funcDeclsString[decl->name] = decl;
  }

  std::map<unsigned, std::shared_ptr<FunctionDeclInfo>> funcDecls;

  std::map<std::string, std::shared_ptr<FunctionDeclInfo>> funcDeclsString;
  std::shared_ptr<FunctionDeclInfo> getFunctionByName(const std::string &name) {
    if (MAP_FIND(funcDeclsString, name)) {
      return funcDeclsString[name];
    }
    return nullptr;
  }

  std::shared_ptr<FunctionDeclInfo> currentFunction;

  bool inCurrentFunction(unsigned lineNumber) const {
    return currentFunction->isInBody(lineNumber);
  }

  std::map<unsigned, std::string> functionCalls;

  void addCall(unsigned lineNum, const std::string &calleeName) {
    functionCalls[lineNum] = calleeName;
  }

  std::map<std::string, unsigned> varDecls;

  void addVarDeclToMap(const std::string name, unsigned lineNum) {
    varDecls[name] = lineNum;
  }

  struct BranchPointInfo {
    unsigned branchPoint;
    std::vector<unsigned> targetLineNumbers;

    unsigned compoundEndLineNum;
 
    unsigned compoundEndColumnNum;

    BranchPointInfo()
        : branchPoint(0), compoundEndLineNum(0), compoundEndColumnNum(0) {}

    unsigned *getBranchPointOut() { return &branchPoint; }
    void addTarget(unsigned target) { targetLineNumbers.push_back(target); }
  };

  unsigned branchCount;
  
  std::stack<BranchPointInfo> branchPointStack;

  std::vector<BranchPointInfo> branchPoints;

 
  void pushNewBranchPoint() { branchPointStack.push(BranchPointInfo()); }

  std::map<unsigned, std::map<unsigned, std::string>> branchDictionary;

  void addBranchesToDictionary();

  void printFoundBranchPoint(const CXCursorKind K);

  void printFoundTargetPoint();

  void printCursorKind(const CXCursorKind K);

  bool isBranchPointOrCallExpr(const CXCursorKind K);

  bool isFunctionPtr(const CXCursor C);

 
  bool compoundStmtFoundYet() const { return !branchPointStack.empty(); }


  bool checkChildAgainstStackTop(CXCursor child);

 
  BranchPointInfo *getCurrentBranch() { return &branchPointStack.top(); }


  void addCompletedBranch();

  void createDictionaryFile();

  void
  insertFunctionBranchPointDecls(std::ofstream &program,
                                 std::shared_ptr<FunctionDeclInfo> function,
                                 int *branchCount);

public:
  
  KeyPointsCollector(const std::string &fileName, bool debug = false);

  ~KeyPointsCollector();


  const std::vector<CXCursor> &getCursorObjs() const { return cursorObjs; }


  const std::map<unsigned, std::shared_ptr<FunctionDeclInfo>> &
  getFuncDecls() const {
    return funcDecls;
  }

  const std::map<unsigned, std::string> &getFuncCalls() const {
    return functionCalls;
  }


  const std::map<std::string, unsigned> &getVarDecls() const {
    return varDecls;
  }

  CXFile *getCXFile() { return &cxFile; }

  
  CXTranslationUnit &getTU() { return translationUnit; }

  const std::map<unsigned, std::map<unsigned, std::string>> &
  getBranchDictionary() {
    return branchDictionary;
  }

  
  void invokeValgrind();

  std::string getBPTrace();
  
  void compileModified();

  void transformProgram();

  
  void collectCursors();

  void executeToolchain();


  unsigned getNumIncludeDirectives() const {
    return includeDirectives.size();
  }
};

#endif 
