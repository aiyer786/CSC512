
#include "FeatureDetector.h"

#include <clang-c/Index.h>
#include <iostream>
#include <fstream>
#include <iterator>

FeatureDetector::FeatureDetector( const std::string &filename, bool debug )
    : filename(std::move(filename)), debug(debug) {

    kpc = new KeyPointsCollector( std::string(filename), false );
    
    kpc->collectCursors();
    cursorObjs = kpc->getCursorObjs();
    varDecls = kpc->getVarDecls();
    count = 0;

    translationUnit =
        clang_parseTranslationUnit( index, filename.c_str(), nullptr, 0,
                                   nullptr, 0, CXTranslationUnit_None );
    cxFile = clang_getFile( translationUnit, filename.c_str() );
}



CXChildVisitResult FeatureDetector::ifStmtBranch(CXCursor current,
                                                      CXCursor parent,
                                                      CXClientData clientData) {

    
    FeatureDetector *instance = static_cast<FeatureDetector *>(clientData);

    if ( !clang_Cursor_isNull( current ) ) {

       
        CXType cursor_type = clang_getCursorType( current );
        CXString type_spelling = clang_getTypeSpelling( cursor_type );

      
        CXSourceLocation location = clang_getCursorLocation( current );
        unsigned line;
        clang_getExpansionLocation( location, &instance->cxFile, &line, nullptr, nullptr );
        line += instance->kpc->getNumIncludeDirectives();
        
        // Cursor Token
        CXToken *cursor_token = clang_getToken( instance->kpc->getTU(), location );
        if ( cursor_token ) {
            CXString token_spelling = clang_getTokenSpelling( instance->translationUnit, *cursor_token );

            if ( parent.kind == CXCursor_IfStmt && ( current.kind == CXCursor_UnexposedExpr 
                                                || current.kind == CXCursor_BinaryOperator ) ) {
                if ( instance->debug ) {
                    CXString parent_kind_spelling = clang_getCursorKindSpelling( parent.kind );
                    CXString current_kind_spelling = clang_getCursorKindSpelling( current.kind );

                    std::cout << "  Kind: " << clang_getCString(parent_kind_spelling) << "\n"
                              << "    Kind: " << clang_getCString(current_kind_spelling) << "\n"
                              << "      Type: " << clang_getCString(type_spelling) << "\n"
                              << "      Token: " << clang_getCString(token_spelling) << "\n"
                              << "      Line " << line << "\n\n";

                    clang_disposeString( parent_kind_spelling );
                    clang_disposeString( current_kind_spelling );
                }

                instance->getDeclLocation( clang_getCString(token_spelling), instance->count++, clang_getCString(type_spelling) );
                clang_disposeString( type_spelling );
                clang_disposeString( token_spelling );
                clang_disposeTokens( instance->translationUnit, cursor_token, 1 );
                return CXChildVisit_Break;
            }

            clang_disposeString( token_spelling );
            clang_disposeTokens( instance->translationUnit, cursor_token, 1 );
        }

        clang_disposeString( type_spelling );
    }
    return CXChildVisit_Recurse;
}

CXChildVisitResult FeatureDetector::forStmtBranch(CXCursor current,
                                                      CXCursor parent,
                                                      CXClientData clientData) {

    
    FeatureDetector *instance = static_cast<FeatureDetector *>(clientData);

    if ( !clang_Cursor_isNull( current ) ) {
        
        
        CXType cursor_type = clang_getCursorType( current );
        CXString type_spelling = clang_getTypeSpelling( cursor_type );

       
        CXSourceLocation location = clang_getCursorLocation( current );
        unsigned line;
        clang_getExpansionLocation( location, &instance->cxFile, &line, nullptr, nullptr );
        line += instance->kpc->getNumIncludeDirectives();

        
        CXToken *cursor_token = clang_getToken( instance->kpc->getTU(), location );
        if ( cursor_token ) {
            CXString token_spelling = clang_getTokenSpelling( instance->translationUnit, *cursor_token );

            if ( (parent.kind == CXCursor_DeclStmt && current.kind == CXCursor_VarDecl) || (current.kind == CXCursor_DeclRefExpr) ) {
                if ( instance->debug ) {
                    CXString parent_kind_spelling = clang_getCursorKindSpelling( parent.kind );
                    CXString current_kind_spelling = clang_getCursorKindSpelling( current.kind );

                    std::cout << "  Kind: " << clang_getCString(parent_kind_spelling) << "\n"
                              << "    Kind: " << clang_getCString(current_kind_spelling) << "\n"
                              << "      Type: " << clang_getCString(type_spelling) << "\n"
                              << "      Token: " << clang_getCString(token_spelling) << "\n"
                              << "      Line " << line << "\n\n";

                    clang_disposeString( parent_kind_spelling );
                    clang_disposeString( current_kind_spelling );
                }

                instance->temp.name = clang_getCString(token_spelling);
            }
            
            if ( ( parent.kind == CXCursor_BinaryOperator || parent.kind == CXCursor_CallExpr ) && current.kind == CXCursor_UnexposedExpr ) {
                if ( instance->debug ) {
                    CXString parent_kind_spelling = clang_getCursorKindSpelling( parent.kind );
                    CXString current_kind_spelling = clang_getCursorKindSpelling( current.kind );

                    std::cout << "  Kind: " << clang_getCString(parent_kind_spelling) << "\n"
                              << "    Kind: " << clang_getCString(current_kind_spelling) << "\n"
                              << "      Type: " << clang_getCString(type_spelling) << "\n"
                              << "      Token: " << clang_getCString(token_spelling) << "\n"
                              << "      Line " << line << "\n\n";
                    
                    clang_disposeString( parent_kind_spelling );
                    clang_disposeString( current_kind_spelling );
                }

                if ( instance->temp.name != clang_getCString(token_spelling) ) {
                    instance->getDeclLocation( clang_getCString(token_spelling), instance->count++, clang_getCString(type_spelling) );
                    clang_disposeString( type_spelling );
                    clang_disposeString( token_spelling );
                    clang_disposeTokens( instance->translationUnit, cursor_token, 1 );
                    return CXChildVisit_Break;
                }
            }
            
            clang_disposeString( token_spelling );
            clang_disposeTokens( instance->translationUnit, cursor_token, 1 );
        }

        clang_disposeString( type_spelling );
    }
    return CXChildVisit_Recurse;
}

CXChildVisitResult FeatureDetector::whileStmtBranch(CXCursor current,
                                                      CXCursor parent,
                                                      CXClientData clientData) {

    
    FeatureDetector *instance = static_cast<FeatureDetector *>(clientData);

    if ( !clang_Cursor_isNull( current ) ) {

        
        CXType cursor_type = clang_getCursorType( current );
        CXString type_spelling = clang_getTypeSpelling( cursor_type );

       
        CXSourceLocation location = clang_getCursorLocation( current );
        unsigned line;
        clang_getExpansionLocation( location, &instance->cxFile, &line, nullptr, nullptr );
        line += instance->kpc->getNumIncludeDirectives();
        
        if ( ( parent.kind == CXCursor_BinaryOperator || parent.kind == CXCursor_CallExpr ) && current.kind == CXCursor_UnexposedExpr ) {
            
            CXToken *cursor_token = clang_getToken( instance->kpc->getTU(), location );
            if ( cursor_token ) {
                CXString token_spelling = clang_getTokenSpelling( instance->kpc->getTU(), *cursor_token );
                if ( instance->debug ) {
                    
                    CXString parent_kind_spelling = clang_getCursorKindSpelling( parent.kind );
                    CXString current_kind_spelling = clang_getCursorKindSpelling( current.kind );

                    std::cout << "  Kind: " << clang_getCString(parent_kind_spelling) << "\n"
                            << "    Kind: " << clang_getCString(current_kind_spelling) << "\n"
                            << "      Type: " << clang_getCString(type_spelling) << "\n"
                            << "      Token: " << clang_getCString(token_spelling) << "\n"
                            << "      Line " << line << "\n\n";

                    clang_disposeString( parent_kind_spelling );
                    clang_disposeString( current_kind_spelling );
                }

                instance->getDeclLocation( clang_getCString(token_spelling), instance->count++, clang_getCString(type_spelling) );
                clang_disposeString( token_spelling );
                clang_disposeTokens( instance->translationUnit, cursor_token, 1 );
                return CXChildVisit_Break;
            }
        }

        clang_disposeString( type_spelling );
    }
    return CXChildVisit_Recurse;
}



void FeatureDetector::getDeclLocation( std::string name, int index, std::string type ) {
    
    
    bool exists = false;
    for ( int i = 0; i < SeminalInputFeatures.size(); i++ ) {
        if ( SeminalInputFeatures[ i ].name == name ) {
            exists = true;
        }
    }

    std::map<std::string, unsigned>::iterator it;
    it = varDecls.find( name );
    if ( !exists ) {
        if ( it != varDecls.end() ) {
            temp.name = it->first;
            temp.line = it->second;
            temp.type = type;
            SeminalInputFeatures.push_back( temp );
        } else if ( debug ) {
            std::cout << "Variable was not found.\n\n";
        }
    } else if ( debug ) {
        std::cout << "Variable is already accounted for.\n\n";
    }
}

void FeatureDetector::printSeminalInputFeatures() {
    for ( int i = 0; i < SeminalInputFeatures.size(); i++ ) {
        if ( SeminalInputFeatures[ i ].type == "FILE *" ) {
            std::cout << "Line " << SeminalInputFeatures[ i ].line << ": size of file "
                  << SeminalInputFeatures[ i ].name << "\n";
        } else {
            std::cout << "Line " << SeminalInputFeatures[ i ].line << ": "
                  << SeminalInputFeatures[ i ].name << "\n";
        }
    }
}

void FeatureDetector::cursorFinder() {

    
    if ( debug ) {
        std::cout << "Variable Declarations: \n";
        for( const std::pair<std::string, unsigned> var : varDecls ) {
            std::cout << var.second << ": " << var.first << "\n";
        }
        std::cout << "\n";
    }

    for ( int i = 0; i < cursorObjs.size(); i++ ) {

        if ( !clang_Cursor_isNull( cursorObjs[i] ) ) {
            if ( debug ) {
                CXString kind_spelling = clang_getCursorKindSpelling( cursorObjs[i].kind );
                std::cout << "Kind: " << clang_getCString(kind_spelling) << "\n";
                clang_disposeString( kind_spelling );
            }

            switch ( cursorObjs[i].kind ) {
                case CXCursor_IfStmt:
                    clang_visitChildren( cursorObjs[i], this->ifStmtBranch, this );
                    break;
                case CXCursor_ForStmt:
                    clang_visitChildren( cursorObjs[i], this->forStmtBranch, this );
                    break;
                case CXCursor_WhileStmt:
                    clang_visitChildren( cursorObjs[i], this->whileStmtBranch, this );
                    break;
                default:
                    break;
            }

            if ( debug ) {
                std::cout << "\n";
            }
        }
    }

    clang_disposeTranslationUnit( translationUnit );
    clang_disposeIndex( index );
    delete kpc;

    printSeminalInputFeatures();
}

void FeatureDetector::findCursorAtLine( int branchLine ) {

    if ( branchLine != -1 ) {
    
        CXSourceLocation location;
        unsigned line;

        
        for ( int i = 0; i < cursorObjs.size(); i++ ) {

            if ( !clang_Cursor_isNull( cursorObjs[i] ) ) {
                if ( debug ) {
                    CXString kind_spelling = clang_getCursorKindSpelling( cursorObjs[i].kind );
                    std::cout << "Kind: " << clang_getCString(kind_spelling) << "\n";
                    clang_disposeString( kind_spelling );
                }

                
                location = clang_getCursorLocation( cursorObjs[i] );
                clang_getExpansionLocation( location, &cxFile, &line, nullptr, nullptr );
                line += kpc->getNumIncludeDirectives();

                if ( line == branchLine ) {
                    switch ( cursorObjs[i].kind ) {
                        case CXCursor_IfStmt:
                            clang_visitChildren( cursorObjs[i], this->ifStmtBranch, this );
                            break;
                        case CXCursor_ForStmt:
                            clang_visitChildren( cursorObjs[i], this->forStmtBranch, this );
                            break;
                        case CXCursor_WhileStmt:
                            clang_visitChildren( cursorObjs[i], this->whileStmtBranch, this );
                            break;
                        default:
                            break;
                    }
                    break;
                }

                if ( debug ) {
                    std::cout << "\n";
                }
            }
        }

    } else {
        std::cout << "No branch points detected.\n";
    }

    clang_disposeTranslationUnit( translationUnit );
    clang_disposeIndex( index );
    delete kpc;

    printSeminalInputFeatures();
}
