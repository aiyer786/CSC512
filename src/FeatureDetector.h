
#include "KeyPointsCollector.h"
#include <string>
#include <vector>
#include <map>
#include <clang-c/Index.h>

class FeatureDetector {

   
    std::string filename;

    
    CXFile cxFile;

    KeyPointsCollector *kpc; 

    std::vector<CXCursor> cursorObjs;

   
    inline static CXIndex index = clang_createIndex(0, 0);

   
    CXTranslationUnit translationUnit;

    static CXChildVisitResult ifStmtBranch(CXCursor current, CXCursor parent, CXClientData clientData);
    static CXChildVisitResult forStmtBranch(CXCursor current, CXCursor parent, CXClientData clientData);
    static CXChildVisitResult whileStmtBranch(CXCursor current, CXCursor parent, CXClientData clientData);

    struct SeminalInputFeature {
        std::string name;
        unsigned line;
        std::string type;
    };

   
    std::vector<SeminalInputFeature> SeminalInputFeatures;
    unsigned count;
    
    std::map<std::string, unsigned> varDecls;

    SeminalInputFeature temp;

    void getDeclLocation( std::string name, int index, std::string type );
    
    void printSeminalInputFeatures();

    bool debug;

public:

    FeatureDetector( const std::string &fileName, bool debug = false );

    void cursorFinder();

    void findCursorAtLine( int branchLine );

};
