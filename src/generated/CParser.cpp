
// Generated from /Users/tom/projects/pcc/grammar/CParser.g4 by ANTLR 4.13.2


#include "CParserVisitor.h"

#include "CParser.h"


using namespace antlrcpp;

using namespace antlr4;

namespace {

struct CParserStaticData final {
  CParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  CParserStaticData(const CParserStaticData&) = delete;
  CParserStaticData(CParserStaticData&&) = delete;
  CParserStaticData& operator=(const CParserStaticData&) = delete;
  CParserStaticData& operator=(CParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag cparserParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<CParserStaticData> cparserParserStaticData = nullptr;

void cparserParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (cparserParserStaticData != nullptr) {
    return;
  }
#else
  assert(cparserParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<CParserStaticData>(
    std::vector<std::string>{
      "program", "typeSpec", "baseTypeSpec", "topLevelDecl", "structDecl", 
      "structMember", "enumDecl", "enumMember", "functionDecl", "returnTypeSpec", 
      "paramList", "param", "globalDecl", "arrayInit", "arrayInitElem", 
      "block", "statement", "simpleStmt", "switchCase", "assignOp", "expr", 
      "condExpr", "argList"
    },
    std::vector<std::string>{
      "", "'int'", "'char'", "'short'", "'long'", "'signed'", "'unsigned'", 
      "'void'", "'sizeof'", "'if'", "'else'", "'while'", "'for'", "'switch'", 
      "'case'", "'default'", "'return'", "'break'", "'continue'", "'goto'", 
      "'extern'", "'static'", "'struct'", "'enum'", "'('", "')'", "'['", 
      "']'", "'{'", "", "'@'", "';'", "','", "'='", "'*'", "'&'", "'-'", 
      "'!'", "'~'", "'/'", "'%'", "'+'", "'<<'", "'>>'", "'<='", "'>='", 
      "'<'", "'>'", "'=='", "'!='", "'^'", "'|'", "'&&'", "'||'", "'+='", 
      "'-='", "'*='", "'/='", "'%='", "'&='", "'^='", "'|='", "'<<='", "'>>='", 
      "'++'", "'--'", "'\\u003F'", "':'", "'.'", "'->'"
    },
    std::vector<std::string>{
      "", "INT_KW", "CHAR_KW", "SHORT_KW", "LONG_KW", "SIGNED_KW", "UNSIGNED_KW", 
      "VOID_KW", "SIZEOF_KW", "IF_KW", "ELSE_KW", "WHILE_KW", "FOR_KW", 
      "SWITCH_KW", "CASE_KW", "DEFAULT_KW", "RETURN_KW", "BREAK_KW", "CONTINUE_KW", 
      "GOTO_KW", "EXTERN_KW", "STATIC_KW", "STRUCT_KW", "ENUM_KW", "LPAREN", 
      "RPAREN", "LBRACK", "RBRACK", "LBRACE", "RBRACE", "AT", "SEMI", "COMMA", 
      "ASSIGN", "STAR", "AMP", "MINUS", "BANG", "TILDE", "SLASH", "PERCENT", 
      "PLUS", "SHL", "SHR", "LE", "GE", "LT", "GT", "EQ", "NE", "CARET", 
      "PIPE", "ANDAND", "OROR", "PLUSEQ", "MINUSEQ", "STAREQ", "SLASHEQ", 
      "PERCENTEQ", "AMPEQ", "CARETEQ", "PIPEEQ", "SHLEQ", "SHREQ", "INC", 
      "DEC", "QUESTION", "COLON", "DOT", "ARROW", "IDENT", "NUMBER", "CHARLIT", 
      "STRINGLIT", "LINE_COMMENT", "BLOCK_COMMENT", "WS", "ASM_BEGIN", "ASM_TEXT", 
      "ASM_END"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,79,603,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
  	21,2,22,7,22,1,0,5,0,48,8,0,10,0,12,0,51,9,0,1,0,1,0,1,1,1,1,5,1,57,8,
  	1,10,1,12,1,60,9,1,1,2,3,2,63,8,2,1,2,1,2,3,2,67,8,2,1,2,1,2,3,2,71,8,
  	2,1,2,3,2,74,8,2,1,2,1,2,3,2,78,8,2,1,2,1,2,1,2,3,2,83,8,2,1,2,3,2,86,
  	8,2,1,2,1,2,3,2,90,8,2,1,2,1,2,1,2,1,2,1,2,3,2,97,8,2,1,3,1,3,1,3,1,3,
  	3,3,103,8,3,1,4,1,4,1,4,1,4,3,4,109,8,4,1,4,1,4,4,4,113,8,4,11,4,12,4,
  	114,1,4,1,4,1,4,1,5,1,5,1,5,1,5,1,5,3,5,125,8,5,1,5,1,5,1,6,1,6,1,6,1,
  	6,1,6,1,6,5,6,135,8,6,10,6,12,6,138,9,6,1,6,3,6,141,8,6,1,6,1,6,1,6,1,
  	7,1,7,1,7,3,7,149,8,7,1,7,3,7,152,8,7,1,8,1,8,1,8,1,8,1,8,3,8,159,8,8,
  	1,8,1,8,1,8,1,8,3,8,165,8,8,1,8,1,8,1,8,1,8,3,8,171,8,8,1,8,1,8,1,8,3,
  	8,176,8,8,1,8,1,8,3,8,180,8,8,3,8,182,8,8,1,9,1,9,3,9,186,8,9,1,10,1,
  	10,1,10,5,10,191,8,10,10,10,12,10,194,9,10,1,11,1,11,1,11,1,12,1,12,1,
  	12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,3,12,213,8,
  	12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,3,12,222,8,12,1,12,1,12,3,12,226,
  	8,12,1,12,1,12,1,12,3,12,231,8,12,1,12,1,12,1,12,1,12,3,12,237,8,12,1,
  	12,1,12,3,12,241,8,12,3,12,243,8,12,1,12,1,12,3,12,247,8,12,1,12,1,12,
  	3,12,251,8,12,1,13,1,13,1,13,1,13,5,13,257,8,13,10,13,12,13,260,9,13,
  	1,13,1,13,1,14,3,14,265,8,14,1,14,1,14,3,14,269,8,14,1,15,1,15,5,15,273,
  	8,15,10,15,12,15,276,9,15,1,15,1,15,1,16,1,16,1,16,1,16,1,16,1,16,1,16,
  	1,16,3,16,288,8,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,3,16,
  	299,8,16,1,16,1,16,3,16,303,8,16,1,16,1,16,3,16,307,8,16,1,16,1,16,1,
  	16,1,16,1,16,1,16,1,16,1,16,5,16,317,8,16,10,16,12,16,320,9,16,1,16,1,
  	16,1,16,1,16,3,16,326,8,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,
  	16,1,16,1,16,1,16,1,16,3,16,341,8,16,1,16,1,16,1,16,1,16,1,16,3,16,348,
  	8,16,1,17,1,17,1,17,1,17,1,17,1,17,1,17,3,17,357,8,17,1,17,1,17,1,17,
  	1,17,3,17,363,8,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,3,17,373,8,
  	17,1,17,1,17,1,17,1,17,1,17,3,17,380,8,17,1,17,1,17,3,17,384,8,17,3,17,
  	386,8,17,1,17,3,17,389,8,17,1,18,1,18,3,18,393,8,18,1,18,1,18,3,18,397,
  	8,18,1,18,1,18,5,18,401,8,18,10,18,12,18,404,9,18,1,18,1,18,1,18,5,18,
  	409,8,18,10,18,12,18,412,9,18,3,18,414,8,18,1,19,1,19,1,20,1,20,1,20,
  	1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,
  	1,20,1,20,1,20,1,20,1,20,1,20,3,20,441,8,20,1,20,1,20,1,20,1,20,1,20,
  	1,20,1,20,1,20,1,20,1,20,3,20,453,8,20,1,20,1,20,1,20,1,20,3,20,459,8,
  	20,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,
  	21,1,21,1,21,3,21,477,8,21,1,21,1,21,1,21,1,21,1,21,1,21,3,21,485,8,21,
  	1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,
  	1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,
  	1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,
  	1,21,1,21,1,21,1,21,1,21,3,21,534,8,21,1,21,1,21,1,21,1,21,1,21,1,21,
  	1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,
  	1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,
  	1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,1,21,3,21,579,8,21,1,21,1,21,
  	1,21,1,21,1,21,1,21,1,21,3,21,588,8,21,5,21,590,8,21,10,21,12,21,593,
  	9,21,1,22,1,22,1,22,5,22,598,8,22,10,22,12,22,601,9,22,1,22,0,1,42,23,
  	0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,0,9,1,
  	0,5,6,2,0,33,33,54,63,1,0,64,65,1,0,36,38,2,0,34,34,39,40,2,0,36,36,41,
  	41,1,0,42,43,1,0,44,47,1,0,48,49,707,0,49,1,0,0,0,2,54,1,0,0,0,4,96,1,
  	0,0,0,6,102,1,0,0,0,8,104,1,0,0,0,10,119,1,0,0,0,12,128,1,0,0,0,14,145,
  	1,0,0,0,16,181,1,0,0,0,18,185,1,0,0,0,20,187,1,0,0,0,22,195,1,0,0,0,24,
  	250,1,0,0,0,26,252,1,0,0,0,28,268,1,0,0,0,30,270,1,0,0,0,32,347,1,0,0,
  	0,34,388,1,0,0,0,36,413,1,0,0,0,38,415,1,0,0,0,40,458,1,0,0,0,42,533,
  	1,0,0,0,44,594,1,0,0,0,46,48,3,6,3,0,47,46,1,0,0,0,48,51,1,0,0,0,49,47,
  	1,0,0,0,49,50,1,0,0,0,50,52,1,0,0,0,51,49,1,0,0,0,52,53,5,0,0,1,53,1,
  	1,0,0,0,54,58,3,4,2,0,55,57,5,34,0,0,56,55,1,0,0,0,57,60,1,0,0,0,58,56,
  	1,0,0,0,58,59,1,0,0,0,59,3,1,0,0,0,60,58,1,0,0,0,61,63,7,0,0,0,62,61,
  	1,0,0,0,62,63,1,0,0,0,63,64,1,0,0,0,64,97,5,2,0,0,65,67,7,0,0,0,66,65,
  	1,0,0,0,66,67,1,0,0,0,67,68,1,0,0,0,68,70,5,3,0,0,69,71,5,1,0,0,70,69,
  	1,0,0,0,70,71,1,0,0,0,71,97,1,0,0,0,72,74,7,0,0,0,73,72,1,0,0,0,73,74,
  	1,0,0,0,74,75,1,0,0,0,75,97,5,1,0,0,76,78,7,0,0,0,77,76,1,0,0,0,77,78,
  	1,0,0,0,78,79,1,0,0,0,79,80,5,4,0,0,80,82,5,4,0,0,81,83,5,1,0,0,82,81,
  	1,0,0,0,82,83,1,0,0,0,83,97,1,0,0,0,84,86,7,0,0,0,85,84,1,0,0,0,85,86,
  	1,0,0,0,86,87,1,0,0,0,87,89,5,4,0,0,88,90,5,1,0,0,89,88,1,0,0,0,89,90,
  	1,0,0,0,90,97,1,0,0,0,91,97,7,0,0,0,92,93,5,22,0,0,93,97,5,70,0,0,94,
  	95,5,23,0,0,95,97,5,70,0,0,96,62,1,0,0,0,96,66,1,0,0,0,96,73,1,0,0,0,
  	96,77,1,0,0,0,96,85,1,0,0,0,96,91,1,0,0,0,96,92,1,0,0,0,96,94,1,0,0,0,
  	97,5,1,0,0,0,98,103,3,16,8,0,99,103,3,24,12,0,100,103,3,8,4,0,101,103,
  	3,12,6,0,102,98,1,0,0,0,102,99,1,0,0,0,102,100,1,0,0,0,102,101,1,0,0,
  	0,103,7,1,0,0,0,104,105,5,22,0,0,105,108,5,70,0,0,106,107,5,30,0,0,107,
  	109,5,70,0,0,108,106,1,0,0,0,108,109,1,0,0,0,109,110,1,0,0,0,110,112,
  	5,28,0,0,111,113,3,10,5,0,112,111,1,0,0,0,113,114,1,0,0,0,114,112,1,0,
  	0,0,114,115,1,0,0,0,115,116,1,0,0,0,116,117,5,29,0,0,117,118,5,31,0,0,
  	118,9,1,0,0,0,119,120,3,2,1,0,120,124,5,70,0,0,121,122,5,26,0,0,122,123,
  	5,71,0,0,123,125,5,27,0,0,124,121,1,0,0,0,124,125,1,0,0,0,125,126,1,0,
  	0,0,126,127,5,31,0,0,127,11,1,0,0,0,128,129,5,23,0,0,129,130,5,70,0,0,
  	130,131,5,28,0,0,131,136,3,14,7,0,132,133,5,32,0,0,133,135,3,14,7,0,134,
  	132,1,0,0,0,135,138,1,0,0,0,136,134,1,0,0,0,136,137,1,0,0,0,137,140,1,
  	0,0,0,138,136,1,0,0,0,139,141,5,32,0,0,140,139,1,0,0,0,140,141,1,0,0,
  	0,141,142,1,0,0,0,142,143,5,29,0,0,143,144,5,31,0,0,144,13,1,0,0,0,145,
  	151,5,70,0,0,146,148,5,33,0,0,147,149,5,36,0,0,148,147,1,0,0,0,148,149,
  	1,0,0,0,149,150,1,0,0,0,150,152,5,71,0,0,151,146,1,0,0,0,151,152,1,0,
  	0,0,152,15,1,0,0,0,153,154,5,20,0,0,154,155,3,18,9,0,155,156,5,70,0,0,
  	156,158,5,24,0,0,157,159,3,20,10,0,158,157,1,0,0,0,158,159,1,0,0,0,159,
  	160,1,0,0,0,160,161,5,25,0,0,161,162,5,31,0,0,162,182,1,0,0,0,163,165,
  	5,21,0,0,164,163,1,0,0,0,164,165,1,0,0,0,165,166,1,0,0,0,166,167,3,18,
  	9,0,167,168,5,70,0,0,168,170,5,24,0,0,169,171,3,20,10,0,170,169,1,0,0,
  	0,170,171,1,0,0,0,171,172,1,0,0,0,172,175,5,25,0,0,173,174,5,30,0,0,174,
  	176,5,71,0,0,175,173,1,0,0,0,175,176,1,0,0,0,176,179,1,0,0,0,177,180,
  	3,30,15,0,178,180,5,31,0,0,179,177,1,0,0,0,179,178,1,0,0,0,180,182,1,
  	0,0,0,181,153,1,0,0,0,181,164,1,0,0,0,182,17,1,0,0,0,183,186,3,2,1,0,
  	184,186,5,7,0,0,185,183,1,0,0,0,185,184,1,0,0,0,186,19,1,0,0,0,187,192,
  	3,22,11,0,188,189,5,32,0,0,189,191,3,22,11,0,190,188,1,0,0,0,191,194,
  	1,0,0,0,192,190,1,0,0,0,192,193,1,0,0,0,193,21,1,0,0,0,194,192,1,0,0,
  	0,195,196,3,2,1,0,196,197,5,70,0,0,197,23,1,0,0,0,198,199,5,20,0,0,199,
  	200,3,2,1,0,200,201,5,70,0,0,201,202,5,26,0,0,202,203,5,71,0,0,203,204,
  	5,27,0,0,204,205,5,31,0,0,205,251,1,0,0,0,206,207,5,20,0,0,207,208,3,
  	2,1,0,208,209,5,70,0,0,209,210,5,31,0,0,210,251,1,0,0,0,211,213,5,21,
  	0,0,212,211,1,0,0,0,212,213,1,0,0,0,213,214,1,0,0,0,214,215,3,2,1,0,215,
  	216,5,70,0,0,216,217,5,26,0,0,217,218,5,71,0,0,218,221,5,27,0,0,219,220,
  	5,33,0,0,220,222,3,26,13,0,221,219,1,0,0,0,221,222,1,0,0,0,222,225,1,
  	0,0,0,223,224,5,30,0,0,224,226,5,71,0,0,225,223,1,0,0,0,225,226,1,0,0,
  	0,226,227,1,0,0,0,227,228,5,31,0,0,228,251,1,0,0,0,229,231,5,21,0,0,230,
  	229,1,0,0,0,230,231,1,0,0,0,231,232,1,0,0,0,232,233,3,2,1,0,233,242,5,
  	70,0,0,234,240,5,33,0,0,235,237,5,36,0,0,236,235,1,0,0,0,236,237,1,0,
  	0,0,237,238,1,0,0,0,238,241,5,71,0,0,239,241,5,70,0,0,240,236,1,0,0,0,
  	240,239,1,0,0,0,241,243,1,0,0,0,242,234,1,0,0,0,242,243,1,0,0,0,243,246,
  	1,0,0,0,244,245,5,30,0,0,245,247,5,71,0,0,246,244,1,0,0,0,246,247,1,0,
  	0,0,247,248,1,0,0,0,248,249,5,31,0,0,249,251,1,0,0,0,250,198,1,0,0,0,
  	250,206,1,0,0,0,250,212,1,0,0,0,250,230,1,0,0,0,251,25,1,0,0,0,252,253,
  	5,28,0,0,253,258,3,28,14,0,254,255,5,32,0,0,255,257,3,28,14,0,256,254,
  	1,0,0,0,257,260,1,0,0,0,258,256,1,0,0,0,258,259,1,0,0,0,259,261,1,0,0,
  	0,260,258,1,0,0,0,261,262,5,29,0,0,262,27,1,0,0,0,263,265,5,36,0,0,264,
  	263,1,0,0,0,264,265,1,0,0,0,265,266,1,0,0,0,266,269,5,71,0,0,267,269,
  	5,70,0,0,268,264,1,0,0,0,268,267,1,0,0,0,269,29,1,0,0,0,270,274,5,28,
  	0,0,271,273,3,32,16,0,272,271,1,0,0,0,273,276,1,0,0,0,274,272,1,0,0,0,
  	274,275,1,0,0,0,275,277,1,0,0,0,276,274,1,0,0,0,277,278,5,29,0,0,278,
  	31,1,0,0,0,279,348,3,30,15,0,280,281,5,9,0,0,281,282,5,24,0,0,282,283,
  	3,40,20,0,283,284,5,25,0,0,284,287,3,32,16,0,285,286,5,10,0,0,286,288,
  	3,32,16,0,287,285,1,0,0,0,287,288,1,0,0,0,288,348,1,0,0,0,289,290,5,11,
  	0,0,290,291,5,24,0,0,291,292,3,40,20,0,292,293,5,25,0,0,293,294,3,32,
  	16,0,294,348,1,0,0,0,295,296,5,12,0,0,296,298,5,24,0,0,297,299,3,34,17,
  	0,298,297,1,0,0,0,298,299,1,0,0,0,299,300,1,0,0,0,300,302,5,31,0,0,301,
  	303,3,40,20,0,302,301,1,0,0,0,302,303,1,0,0,0,303,304,1,0,0,0,304,306,
  	5,31,0,0,305,307,3,34,17,0,306,305,1,0,0,0,306,307,1,0,0,0,307,308,1,
  	0,0,0,308,309,5,25,0,0,309,348,3,32,16,0,310,311,5,13,0,0,311,312,5,24,
  	0,0,312,313,3,40,20,0,313,314,5,25,0,0,314,318,5,28,0,0,315,317,3,36,
  	18,0,316,315,1,0,0,0,317,320,1,0,0,0,318,316,1,0,0,0,318,319,1,0,0,0,
  	319,321,1,0,0,0,320,318,1,0,0,0,321,322,5,29,0,0,322,348,1,0,0,0,323,
  	325,5,16,0,0,324,326,3,40,20,0,325,324,1,0,0,0,325,326,1,0,0,0,326,327,
  	1,0,0,0,327,348,5,31,0,0,328,329,5,17,0,0,329,348,5,31,0,0,330,331,5,
  	18,0,0,331,348,5,31,0,0,332,333,5,19,0,0,333,334,5,70,0,0,334,348,5,31,
  	0,0,335,336,5,70,0,0,336,337,5,67,0,0,337,348,3,32,16,0,338,340,5,77,
  	0,0,339,341,5,78,0,0,340,339,1,0,0,0,340,341,1,0,0,0,341,342,1,0,0,0,
  	342,348,5,79,0,0,343,344,3,34,17,0,344,345,5,31,0,0,345,348,1,0,0,0,346,
  	348,5,31,0,0,347,279,1,0,0,0,347,280,1,0,0,0,347,289,1,0,0,0,347,295,
  	1,0,0,0,347,310,1,0,0,0,347,323,1,0,0,0,347,328,1,0,0,0,347,330,1,0,0,
  	0,347,332,1,0,0,0,347,335,1,0,0,0,347,338,1,0,0,0,347,343,1,0,0,0,347,
  	346,1,0,0,0,348,33,1,0,0,0,349,350,3,2,1,0,350,351,5,70,0,0,351,352,5,
  	26,0,0,352,353,5,71,0,0,353,356,5,27,0,0,354,355,5,33,0,0,355,357,3,26,
  	13,0,356,354,1,0,0,0,356,357,1,0,0,0,357,389,1,0,0,0,358,359,3,2,1,0,
  	359,362,5,70,0,0,360,361,5,33,0,0,361,363,3,40,20,0,362,360,1,0,0,0,362,
  	363,1,0,0,0,363,389,1,0,0,0,364,365,5,21,0,0,365,366,3,2,1,0,366,367,
  	5,70,0,0,367,368,5,26,0,0,368,369,5,71,0,0,369,372,5,27,0,0,370,371,5,
  	33,0,0,371,373,3,26,13,0,372,370,1,0,0,0,372,373,1,0,0,0,373,389,1,0,
  	0,0,374,375,5,21,0,0,375,376,3,2,1,0,376,385,5,70,0,0,377,383,5,33,0,
  	0,378,380,5,36,0,0,379,378,1,0,0,0,379,380,1,0,0,0,380,381,1,0,0,0,381,
  	384,5,71,0,0,382,384,5,70,0,0,383,379,1,0,0,0,383,382,1,0,0,0,384,386,
  	1,0,0,0,385,377,1,0,0,0,385,386,1,0,0,0,386,389,1,0,0,0,387,389,3,40,
  	20,0,388,349,1,0,0,0,388,358,1,0,0,0,388,364,1,0,0,0,388,374,1,0,0,0,
  	388,387,1,0,0,0,389,35,1,0,0,0,390,396,5,14,0,0,391,393,5,36,0,0,392,
  	391,1,0,0,0,392,393,1,0,0,0,393,394,1,0,0,0,394,397,5,71,0,0,395,397,
  	5,70,0,0,396,392,1,0,0,0,396,395,1,0,0,0,397,398,1,0,0,0,398,402,5,67,
  	0,0,399,401,3,32,16,0,400,399,1,0,0,0,401,404,1,0,0,0,402,400,1,0,0,0,
  	402,403,1,0,0,0,403,414,1,0,0,0,404,402,1,0,0,0,405,406,5,15,0,0,406,
  	410,5,67,0,0,407,409,3,32,16,0,408,407,1,0,0,0,409,412,1,0,0,0,410,408,
  	1,0,0,0,410,411,1,0,0,0,411,414,1,0,0,0,412,410,1,0,0,0,413,390,1,0,0,
  	0,413,405,1,0,0,0,414,37,1,0,0,0,415,416,7,1,0,0,416,39,1,0,0,0,417,418,
  	5,34,0,0,418,419,3,42,21,0,419,420,3,38,19,0,420,421,3,40,20,0,421,459,
  	1,0,0,0,422,423,5,70,0,0,423,424,5,26,0,0,424,425,3,40,20,0,425,426,5,
  	27,0,0,426,427,3,38,19,0,427,428,3,40,20,0,428,459,1,0,0,0,429,430,5,
  	70,0,0,430,431,3,38,19,0,431,432,3,40,20,0,432,459,1,0,0,0,433,434,3,
  	42,21,0,434,435,5,68,0,0,435,440,5,70,0,0,436,437,5,26,0,0,437,438,3,
  	40,20,0,438,439,5,27,0,0,439,441,1,0,0,0,440,436,1,0,0,0,440,441,1,0,
  	0,0,441,442,1,0,0,0,442,443,3,38,19,0,443,444,3,40,20,0,444,459,1,0,0,
  	0,445,446,3,42,21,0,446,447,5,69,0,0,447,452,5,70,0,0,448,449,5,26,0,
  	0,449,450,3,40,20,0,450,451,5,27,0,0,451,453,1,0,0,0,452,448,1,0,0,0,
  	452,453,1,0,0,0,453,454,1,0,0,0,454,455,3,38,19,0,455,456,3,40,20,0,456,
  	459,1,0,0,0,457,459,3,42,21,0,458,417,1,0,0,0,458,422,1,0,0,0,458,429,
  	1,0,0,0,458,433,1,0,0,0,458,445,1,0,0,0,458,457,1,0,0,0,459,41,1,0,0,
  	0,460,461,6,21,-1,0,461,462,5,70,0,0,462,534,7,2,0,0,463,464,5,70,0,0,
  	464,465,5,26,0,0,465,466,3,40,20,0,466,467,5,27,0,0,467,468,7,2,0,0,468,
  	534,1,0,0,0,469,470,5,34,0,0,470,471,3,42,21,0,471,472,7,2,0,0,472,534,
  	1,0,0,0,473,474,5,70,0,0,474,476,5,24,0,0,475,477,3,44,22,0,476,475,1,
  	0,0,0,476,477,1,0,0,0,477,478,1,0,0,0,478,534,5,25,0,0,479,480,5,24,0,
  	0,480,481,3,40,20,0,481,482,5,25,0,0,482,484,5,24,0,0,483,485,3,44,22,
  	0,484,483,1,0,0,0,484,485,1,0,0,0,485,486,1,0,0,0,486,487,5,25,0,0,487,
  	534,1,0,0,0,488,489,5,70,0,0,489,490,5,26,0,0,490,491,3,40,20,0,491,492,
  	5,27,0,0,492,534,1,0,0,0,493,534,5,70,0,0,494,534,5,71,0,0,495,534,5,
  	72,0,0,496,534,5,73,0,0,497,498,5,24,0,0,498,499,3,40,20,0,499,500,5,
  	25,0,0,500,534,1,0,0,0,501,502,5,24,0,0,502,503,3,2,1,0,503,504,5,25,
  	0,0,504,505,3,42,21,22,505,534,1,0,0,0,506,507,5,8,0,0,507,508,5,24,0,
  	0,508,509,3,2,1,0,509,510,5,25,0,0,510,534,1,0,0,0,511,512,5,8,0,0,512,
  	513,5,24,0,0,513,514,3,40,20,0,514,515,5,25,0,0,515,534,1,0,0,0,516,517,
  	5,34,0,0,517,534,3,42,21,19,518,519,5,35,0,0,519,534,3,42,21,18,520,521,
  	7,3,0,0,521,534,3,42,21,15,522,523,7,2,0,0,523,534,5,70,0,0,524,525,7,
  	2,0,0,525,526,5,70,0,0,526,527,5,26,0,0,527,528,3,40,20,0,528,529,5,27,
  	0,0,529,534,1,0,0,0,530,531,7,2,0,0,531,532,5,34,0,0,532,534,3,42,21,
  	12,533,460,1,0,0,0,533,463,1,0,0,0,533,469,1,0,0,0,533,473,1,0,0,0,533,
  	479,1,0,0,0,533,488,1,0,0,0,533,493,1,0,0,0,533,494,1,0,0,0,533,495,1,
  	0,0,0,533,496,1,0,0,0,533,497,1,0,0,0,533,501,1,0,0,0,533,506,1,0,0,0,
  	533,511,1,0,0,0,533,516,1,0,0,0,533,518,1,0,0,0,533,520,1,0,0,0,533,522,
  	1,0,0,0,533,524,1,0,0,0,533,530,1,0,0,0,534,591,1,0,0,0,535,536,10,11,
  	0,0,536,537,7,4,0,0,537,590,3,42,21,12,538,539,10,10,0,0,539,540,7,5,
  	0,0,540,590,3,42,21,11,541,542,10,9,0,0,542,543,7,6,0,0,543,590,3,42,
  	21,10,544,545,10,8,0,0,545,546,7,7,0,0,546,590,3,42,21,9,547,548,10,7,
  	0,0,548,549,7,8,0,0,549,590,3,42,21,8,550,551,10,6,0,0,551,552,5,35,0,
  	0,552,590,3,42,21,7,553,554,10,5,0,0,554,555,5,50,0,0,555,590,3,42,21,
  	6,556,557,10,4,0,0,557,558,5,51,0,0,558,590,3,42,21,5,559,560,10,3,0,
  	0,560,561,5,52,0,0,561,590,3,42,21,4,562,563,10,2,0,0,563,564,5,53,0,
  	0,564,590,3,42,21,3,565,566,10,1,0,0,566,567,5,66,0,0,567,568,3,40,20,
  	0,568,569,5,67,0,0,569,570,3,42,21,1,570,590,1,0,0,0,571,572,10,17,0,
  	0,572,573,5,68,0,0,573,578,5,70,0,0,574,575,5,26,0,0,575,576,3,40,20,
  	0,576,577,5,27,0,0,577,579,1,0,0,0,578,574,1,0,0,0,578,579,1,0,0,0,579,
  	590,1,0,0,0,580,581,10,16,0,0,581,582,5,69,0,0,582,587,5,70,0,0,583,584,
  	5,26,0,0,584,585,3,40,20,0,585,586,5,27,0,0,586,588,1,0,0,0,587,583,1,
  	0,0,0,587,588,1,0,0,0,588,590,1,0,0,0,589,535,1,0,0,0,589,538,1,0,0,0,
  	589,541,1,0,0,0,589,544,1,0,0,0,589,547,1,0,0,0,589,550,1,0,0,0,589,553,
  	1,0,0,0,589,556,1,0,0,0,589,559,1,0,0,0,589,562,1,0,0,0,589,565,1,0,0,
  	0,589,571,1,0,0,0,589,580,1,0,0,0,590,593,1,0,0,0,591,589,1,0,0,0,591,
  	592,1,0,0,0,592,43,1,0,0,0,593,591,1,0,0,0,594,599,3,40,20,0,595,596,
  	5,32,0,0,596,598,3,40,20,0,597,595,1,0,0,0,598,601,1,0,0,0,599,597,1,
  	0,0,0,599,600,1,0,0,0,600,45,1,0,0,0,601,599,1,0,0,0,71,49,58,62,66,70,
  	73,77,82,85,89,96,102,108,114,124,136,140,148,151,158,164,170,175,179,
  	181,185,192,212,221,225,230,236,240,242,246,250,258,264,268,274,287,298,
  	302,306,318,325,340,347,356,362,372,379,383,385,388,392,396,402,410,413,
  	440,452,458,476,484,533,578,587,589,591,599
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  cparserParserStaticData = std::move(staticData);
}

}

CParser::CParser(TokenStream *input) : CParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

CParser::CParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  CParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *cparserParserStaticData->atn, cparserParserStaticData->decisionToDFA, cparserParserStaticData->sharedContextCache, options);
}

CParser::~CParser() {
  delete _interpreter;
}

const atn::ATN& CParser::getATN() const {
  return *cparserParserStaticData->atn;
}

std::string CParser::getGrammarFileName() const {
  return "CParser.g4";
}

const std::vector<std::string>& CParser::getRuleNames() const {
  return cparserParserStaticData->ruleNames;
}

const dfa::Vocabulary& CParser::getVocabulary() const {
  return cparserParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView CParser::getSerializedATN() const {
  return cparserParserStaticData->serializedATN;
}


//----------------- ProgramContext ------------------------------------------------------------------

CParser::ProgramContext::ProgramContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* CParser::ProgramContext::EOF() {
  return getToken(CParser::EOF, 0);
}

std::vector<CParser::TopLevelDeclContext *> CParser::ProgramContext::topLevelDecl() {
  return getRuleContexts<CParser::TopLevelDeclContext>();
}

CParser::TopLevelDeclContext* CParser::ProgramContext::topLevelDecl(size_t i) {
  return getRuleContext<CParser::TopLevelDeclContext>(i);
}


size_t CParser::ProgramContext::getRuleIndex() const {
  return CParser::RuleProgram;
}


std::any CParser::ProgramContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitProgram(this);
  else
    return visitor->visitChildren(this);
}

CParser::ProgramContext* CParser::program() {
  ProgramContext *_localctx = _tracker.createInstance<ProgramContext>(_ctx, getState());
  enterRule(_localctx, 0, CParser::RuleProgram);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(49);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 15728894) != 0)) {
      setState(46);
      topLevelDecl();
      setState(51);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(52);
    match(CParser::EOF);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TypeSpecContext ------------------------------------------------------------------

CParser::TypeSpecContext::TypeSpecContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

CParser::BaseTypeSpecContext* CParser::TypeSpecContext::baseTypeSpec() {
  return getRuleContext<CParser::BaseTypeSpecContext>(0);
}

std::vector<tree::TerminalNode *> CParser::TypeSpecContext::STAR() {
  return getTokens(CParser::STAR);
}

tree::TerminalNode* CParser::TypeSpecContext::STAR(size_t i) {
  return getToken(CParser::STAR, i);
}


size_t CParser::TypeSpecContext::getRuleIndex() const {
  return CParser::RuleTypeSpec;
}


std::any CParser::TypeSpecContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitTypeSpec(this);
  else
    return visitor->visitChildren(this);
}

CParser::TypeSpecContext* CParser::typeSpec() {
  TypeSpecContext *_localctx = _tracker.createInstance<TypeSpecContext>(_ctx, getState());
  enterRule(_localctx, 2, CParser::RuleTypeSpec);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(54);
    baseTypeSpec();
    setState(58);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::STAR) {
      setState(55);
      match(CParser::STAR);
      setState(60);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BaseTypeSpecContext ------------------------------------------------------------------

CParser::BaseTypeSpecContext::BaseTypeSpecContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::BaseTypeSpecContext::getRuleIndex() const {
  return CParser::RuleBaseTypeSpec;
}

void CParser::BaseTypeSpecContext::copyFrom(BaseTypeSpecContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- CharTypeContext ------------------------------------------------------------------

tree::TerminalNode* CParser::CharTypeContext::CHAR_KW() {
  return getToken(CParser::CHAR_KW, 0);
}

tree::TerminalNode* CParser::CharTypeContext::SIGNED_KW() {
  return getToken(CParser::SIGNED_KW, 0);
}

tree::TerminalNode* CParser::CharTypeContext::UNSIGNED_KW() {
  return getToken(CParser::UNSIGNED_KW, 0);
}

CParser::CharTypeContext::CharTypeContext(BaseTypeSpecContext *ctx) { copyFrom(ctx); }


std::any CParser::CharTypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitCharType(this);
  else
    return visitor->visitChildren(this);
}
//----------------- SignedOrUnsignedTypeContext ------------------------------------------------------------------

tree::TerminalNode* CParser::SignedOrUnsignedTypeContext::SIGNED_KW() {
  return getToken(CParser::SIGNED_KW, 0);
}

tree::TerminalNode* CParser::SignedOrUnsignedTypeContext::UNSIGNED_KW() {
  return getToken(CParser::UNSIGNED_KW, 0);
}

CParser::SignedOrUnsignedTypeContext::SignedOrUnsignedTypeContext(BaseTypeSpecContext *ctx) { copyFrom(ctx); }


std::any CParser::SignedOrUnsignedTypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitSignedOrUnsignedType(this);
  else
    return visitor->visitChildren(this);
}
//----------------- StructTypeContext ------------------------------------------------------------------

tree::TerminalNode* CParser::StructTypeContext::STRUCT_KW() {
  return getToken(CParser::STRUCT_KW, 0);
}

tree::TerminalNode* CParser::StructTypeContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

CParser::StructTypeContext::StructTypeContext(BaseTypeSpecContext *ctx) { copyFrom(ctx); }


std::any CParser::StructTypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitStructType(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IntTypeContext ------------------------------------------------------------------

tree::TerminalNode* CParser::IntTypeContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
}

tree::TerminalNode* CParser::IntTypeContext::SIGNED_KW() {
  return getToken(CParser::SIGNED_KW, 0);
}

tree::TerminalNode* CParser::IntTypeContext::UNSIGNED_KW() {
  return getToken(CParser::UNSIGNED_KW, 0);
}

CParser::IntTypeContext::IntTypeContext(BaseTypeSpecContext *ctx) { copyFrom(ctx); }


std::any CParser::IntTypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitIntType(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ShortTypeContext ------------------------------------------------------------------

tree::TerminalNode* CParser::ShortTypeContext::SHORT_KW() {
  return getToken(CParser::SHORT_KW, 0);
}

tree::TerminalNode* CParser::ShortTypeContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
}

tree::TerminalNode* CParser::ShortTypeContext::SIGNED_KW() {
  return getToken(CParser::SIGNED_KW, 0);
}

tree::TerminalNode* CParser::ShortTypeContext::UNSIGNED_KW() {
  return getToken(CParser::UNSIGNED_KW, 0);
}

CParser::ShortTypeContext::ShortTypeContext(BaseTypeSpecContext *ctx) { copyFrom(ctx); }


std::any CParser::ShortTypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitShortType(this);
  else
    return visitor->visitChildren(this);
}
//----------------- EnumTypeContext ------------------------------------------------------------------

tree::TerminalNode* CParser::EnumTypeContext::ENUM_KW() {
  return getToken(CParser::ENUM_KW, 0);
}

tree::TerminalNode* CParser::EnumTypeContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

CParser::EnumTypeContext::EnumTypeContext(BaseTypeSpecContext *ctx) { copyFrom(ctx); }


std::any CParser::EnumTypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitEnumType(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LongLongTypeContext ------------------------------------------------------------------

std::vector<tree::TerminalNode *> CParser::LongLongTypeContext::LONG_KW() {
  return getTokens(CParser::LONG_KW);
}

tree::TerminalNode* CParser::LongLongTypeContext::LONG_KW(size_t i) {
  return getToken(CParser::LONG_KW, i);
}

tree::TerminalNode* CParser::LongLongTypeContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
}

tree::TerminalNode* CParser::LongLongTypeContext::SIGNED_KW() {
  return getToken(CParser::SIGNED_KW, 0);
}

tree::TerminalNode* CParser::LongLongTypeContext::UNSIGNED_KW() {
  return getToken(CParser::UNSIGNED_KW, 0);
}

CParser::LongLongTypeContext::LongLongTypeContext(BaseTypeSpecContext *ctx) { copyFrom(ctx); }


std::any CParser::LongLongTypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitLongLongType(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LongTypeContext ------------------------------------------------------------------

tree::TerminalNode* CParser::LongTypeContext::LONG_KW() {
  return getToken(CParser::LONG_KW, 0);
}

tree::TerminalNode* CParser::LongTypeContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
}

tree::TerminalNode* CParser::LongTypeContext::SIGNED_KW() {
  return getToken(CParser::SIGNED_KW, 0);
}

tree::TerminalNode* CParser::LongTypeContext::UNSIGNED_KW() {
  return getToken(CParser::UNSIGNED_KW, 0);
}

CParser::LongTypeContext::LongTypeContext(BaseTypeSpecContext *ctx) { copyFrom(ctx); }


std::any CParser::LongTypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitLongType(this);
  else
    return visitor->visitChildren(this);
}
CParser::BaseTypeSpecContext* CParser::baseTypeSpec() {
  BaseTypeSpecContext *_localctx = _tracker.createInstance<BaseTypeSpecContext>(_ctx, getState());
  enterRule(_localctx, 4, CParser::RuleBaseTypeSpec);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(96);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 10, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::CharTypeContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(62);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(61);
        _la = _input->LA(1);
        if (!(_la == CParser::SIGNED_KW

        || _la == CParser::UNSIGNED_KW)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
      setState(64);
      match(CParser::CHAR_KW);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::ShortTypeContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(66);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(65);
        _la = _input->LA(1);
        if (!(_la == CParser::SIGNED_KW

        || _la == CParser::UNSIGNED_KW)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
      setState(68);
      match(CParser::SHORT_KW);
      setState(70);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::INT_KW) {
        setState(69);
        match(CParser::INT_KW);
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::IntTypeContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(73);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(72);
        _la = _input->LA(1);
        if (!(_la == CParser::SIGNED_KW

        || _la == CParser::UNSIGNED_KW)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
      setState(75);
      match(CParser::INT_KW);
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::LongLongTypeContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(77);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(76);
        _la = _input->LA(1);
        if (!(_la == CParser::SIGNED_KW

        || _la == CParser::UNSIGNED_KW)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
      setState(79);
      match(CParser::LONG_KW);
      setState(80);
      match(CParser::LONG_KW);
      setState(82);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::INT_KW) {
        setState(81);
        match(CParser::INT_KW);
      }
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<CParser::LongTypeContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(85);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(84);
        _la = _input->LA(1);
        if (!(_la == CParser::SIGNED_KW

        || _la == CParser::UNSIGNED_KW)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
      }
      setState(87);
      match(CParser::LONG_KW);
      setState(89);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::INT_KW) {
        setState(88);
        match(CParser::INT_KW);
      }
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<CParser::SignedOrUnsignedTypeContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(91);
      _la = _input->LA(1);
      if (!(_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<CParser::StructTypeContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(92);
      match(CParser::STRUCT_KW);
      setState(93);
      match(CParser::IDENT);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<CParser::EnumTypeContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(94);
      match(CParser::ENUM_KW);
      setState(95);
      match(CParser::IDENT);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TopLevelDeclContext ------------------------------------------------------------------

CParser::TopLevelDeclContext::TopLevelDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

CParser::FunctionDeclContext* CParser::TopLevelDeclContext::functionDecl() {
  return getRuleContext<CParser::FunctionDeclContext>(0);
}

CParser::GlobalDeclContext* CParser::TopLevelDeclContext::globalDecl() {
  return getRuleContext<CParser::GlobalDeclContext>(0);
}

CParser::StructDeclContext* CParser::TopLevelDeclContext::structDecl() {
  return getRuleContext<CParser::StructDeclContext>(0);
}

CParser::EnumDeclContext* CParser::TopLevelDeclContext::enumDecl() {
  return getRuleContext<CParser::EnumDeclContext>(0);
}


size_t CParser::TopLevelDeclContext::getRuleIndex() const {
  return CParser::RuleTopLevelDecl;
}


std::any CParser::TopLevelDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitTopLevelDecl(this);
  else
    return visitor->visitChildren(this);
}

CParser::TopLevelDeclContext* CParser::topLevelDecl() {
  TopLevelDeclContext *_localctx = _tracker.createInstance<TopLevelDeclContext>(_ctx, getState());
  enterRule(_localctx, 6, CParser::RuleTopLevelDecl);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(102);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 11, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(98);
      functionDecl();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(99);
      globalDecl();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(100);
      structDecl();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(101);
      enumDecl();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StructDeclContext ------------------------------------------------------------------

CParser::StructDeclContext::StructDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* CParser::StructDeclContext::STRUCT_KW() {
  return getToken(CParser::STRUCT_KW, 0);
}

std::vector<tree::TerminalNode *> CParser::StructDeclContext::IDENT() {
  return getTokens(CParser::IDENT);
}

tree::TerminalNode* CParser::StructDeclContext::IDENT(size_t i) {
  return getToken(CParser::IDENT, i);
}

tree::TerminalNode* CParser::StructDeclContext::LBRACE() {
  return getToken(CParser::LBRACE, 0);
}

tree::TerminalNode* CParser::StructDeclContext::RBRACE() {
  return getToken(CParser::RBRACE, 0);
}

tree::TerminalNode* CParser::StructDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

tree::TerminalNode* CParser::StructDeclContext::AT() {
  return getToken(CParser::AT, 0);
}

std::vector<CParser::StructMemberContext *> CParser::StructDeclContext::structMember() {
  return getRuleContexts<CParser::StructMemberContext>();
}

CParser::StructMemberContext* CParser::StructDeclContext::structMember(size_t i) {
  return getRuleContext<CParser::StructMemberContext>(i);
}


size_t CParser::StructDeclContext::getRuleIndex() const {
  return CParser::RuleStructDecl;
}


std::any CParser::StructDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitStructDecl(this);
  else
    return visitor->visitChildren(this);
}

CParser::StructDeclContext* CParser::structDecl() {
  StructDeclContext *_localctx = _tracker.createInstance<StructDeclContext>(_ctx, getState());
  enterRule(_localctx, 8, CParser::RuleStructDecl);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(104);
    match(CParser::STRUCT_KW);
    setState(105);
    match(CParser::IDENT);
    setState(108);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == CParser::AT) {
      setState(106);
      match(CParser::AT);
      setState(107);
      antlrcpp::downCast<StructDeclContext *>(_localctx)->packedTag = match(CParser::IDENT);
    }
    setState(110);
    match(CParser::LBRACE);
    setState(112); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(111);
      structMember();
      setState(114); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 12583038) != 0));
    setState(116);
    match(CParser::RBRACE);
    setState(117);
    match(CParser::SEMI);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StructMemberContext ------------------------------------------------------------------

CParser::StructMemberContext::StructMemberContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

CParser::TypeSpecContext* CParser::StructMemberContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::StructMemberContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::StructMemberContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

tree::TerminalNode* CParser::StructMemberContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::StructMemberContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

tree::TerminalNode* CParser::StructMemberContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}


size_t CParser::StructMemberContext::getRuleIndex() const {
  return CParser::RuleStructMember;
}


std::any CParser::StructMemberContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitStructMember(this);
  else
    return visitor->visitChildren(this);
}

CParser::StructMemberContext* CParser::structMember() {
  StructMemberContext *_localctx = _tracker.createInstance<StructMemberContext>(_ctx, getState());
  enterRule(_localctx, 10, CParser::RuleStructMember);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(119);
    typeSpec();
    setState(120);
    match(CParser::IDENT);
    setState(124);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == CParser::LBRACK) {
      setState(121);
      match(CParser::LBRACK);
      setState(122);
      antlrcpp::downCast<StructMemberContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(123);
      match(CParser::RBRACK);
    }
    setState(126);
    match(CParser::SEMI);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- EnumDeclContext ------------------------------------------------------------------

CParser::EnumDeclContext::EnumDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* CParser::EnumDeclContext::ENUM_KW() {
  return getToken(CParser::ENUM_KW, 0);
}

tree::TerminalNode* CParser::EnumDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::EnumDeclContext::LBRACE() {
  return getToken(CParser::LBRACE, 0);
}

std::vector<CParser::EnumMemberContext *> CParser::EnumDeclContext::enumMember() {
  return getRuleContexts<CParser::EnumMemberContext>();
}

CParser::EnumMemberContext* CParser::EnumDeclContext::enumMember(size_t i) {
  return getRuleContext<CParser::EnumMemberContext>(i);
}

tree::TerminalNode* CParser::EnumDeclContext::RBRACE() {
  return getToken(CParser::RBRACE, 0);
}

tree::TerminalNode* CParser::EnumDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

std::vector<tree::TerminalNode *> CParser::EnumDeclContext::COMMA() {
  return getTokens(CParser::COMMA);
}

tree::TerminalNode* CParser::EnumDeclContext::COMMA(size_t i) {
  return getToken(CParser::COMMA, i);
}


size_t CParser::EnumDeclContext::getRuleIndex() const {
  return CParser::RuleEnumDecl;
}


std::any CParser::EnumDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitEnumDecl(this);
  else
    return visitor->visitChildren(this);
}

CParser::EnumDeclContext* CParser::enumDecl() {
  EnumDeclContext *_localctx = _tracker.createInstance<EnumDeclContext>(_ctx, getState());
  enterRule(_localctx, 12, CParser::RuleEnumDecl);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(128);
    match(CParser::ENUM_KW);
    setState(129);
    match(CParser::IDENT);
    setState(130);
    match(CParser::LBRACE);
    setState(131);
    enumMember();
    setState(136);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 15, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(132);
        match(CParser::COMMA);
        setState(133);
        enumMember(); 
      }
      setState(138);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 15, _ctx);
    }
    setState(140);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == CParser::COMMA) {
      setState(139);
      match(CParser::COMMA);
    }
    setState(142);
    match(CParser::RBRACE);
    setState(143);
    match(CParser::SEMI);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- EnumMemberContext ------------------------------------------------------------------

CParser::EnumMemberContext::EnumMemberContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* CParser::EnumMemberContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::EnumMemberContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

tree::TerminalNode* CParser::EnumMemberContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

tree::TerminalNode* CParser::EnumMemberContext::MINUS() {
  return getToken(CParser::MINUS, 0);
}


size_t CParser::EnumMemberContext::getRuleIndex() const {
  return CParser::RuleEnumMember;
}


std::any CParser::EnumMemberContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitEnumMember(this);
  else
    return visitor->visitChildren(this);
}

CParser::EnumMemberContext* CParser::enumMember() {
  EnumMemberContext *_localctx = _tracker.createInstance<EnumMemberContext>(_ctx, getState());
  enterRule(_localctx, 14, CParser::RuleEnumMember);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(145);
    match(CParser::IDENT);
    setState(151);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == CParser::ASSIGN) {
      setState(146);
      match(CParser::ASSIGN);
      setState(148);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::MINUS) {
        setState(147);
        antlrcpp::downCast<EnumMemberContext *>(_localctx)->sign = match(CParser::MINUS);
      }
      setState(150);
      antlrcpp::downCast<EnumMemberContext *>(_localctx)->val = match(CParser::NUMBER);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- FunctionDeclContext ------------------------------------------------------------------

CParser::FunctionDeclContext::FunctionDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::FunctionDeclContext::getRuleIndex() const {
  return CParser::RuleFunctionDecl;
}

void CParser::FunctionDeclContext::copyFrom(FunctionDeclContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- ExternFunctionDeclContext ------------------------------------------------------------------

tree::TerminalNode* CParser::ExternFunctionDeclContext::EXTERN_KW() {
  return getToken(CParser::EXTERN_KW, 0);
}

CParser::ReturnTypeSpecContext* CParser::ExternFunctionDeclContext::returnTypeSpec() {
  return getRuleContext<CParser::ReturnTypeSpecContext>(0);
}

tree::TerminalNode* CParser::ExternFunctionDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::ExternFunctionDeclContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

tree::TerminalNode* CParser::ExternFunctionDeclContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

tree::TerminalNode* CParser::ExternFunctionDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::ParamListContext* CParser::ExternFunctionDeclContext::paramList() {
  return getRuleContext<CParser::ParamListContext>(0);
}

CParser::ExternFunctionDeclContext::ExternFunctionDeclContext(FunctionDeclContext *ctx) { copyFrom(ctx); }


std::any CParser::ExternFunctionDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitExternFunctionDecl(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LocalFunctionDeclContext ------------------------------------------------------------------

CParser::ReturnTypeSpecContext* CParser::LocalFunctionDeclContext::returnTypeSpec() {
  return getRuleContext<CParser::ReturnTypeSpecContext>(0);
}

tree::TerminalNode* CParser::LocalFunctionDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::LocalFunctionDeclContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

tree::TerminalNode* CParser::LocalFunctionDeclContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::BlockContext* CParser::LocalFunctionDeclContext::block() {
  return getRuleContext<CParser::BlockContext>(0);
}

tree::TerminalNode* CParser::LocalFunctionDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::ParamListContext* CParser::LocalFunctionDeclContext::paramList() {
  return getRuleContext<CParser::ParamListContext>(0);
}

tree::TerminalNode* CParser::LocalFunctionDeclContext::AT() {
  return getToken(CParser::AT, 0);
}

tree::TerminalNode* CParser::LocalFunctionDeclContext::STATIC_KW() {
  return getToken(CParser::STATIC_KW, 0);
}

tree::TerminalNode* CParser::LocalFunctionDeclContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

CParser::LocalFunctionDeclContext::LocalFunctionDeclContext(FunctionDeclContext *ctx) { copyFrom(ctx); }


std::any CParser::LocalFunctionDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitLocalFunctionDecl(this);
  else
    return visitor->visitChildren(this);
}
CParser::FunctionDeclContext* CParser::functionDecl() {
  FunctionDeclContext *_localctx = _tracker.createInstance<FunctionDeclContext>(_ctx, getState());
  enterRule(_localctx, 16, CParser::RuleFunctionDecl);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(181);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case CParser::EXTERN_KW: {
        _localctx = _tracker.createInstance<CParser::ExternFunctionDeclContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(153);
        match(CParser::EXTERN_KW);
        setState(154);
        returnTypeSpec();
        setState(155);
        match(CParser::IDENT);
        setState(156);
        match(CParser::LPAREN);
        setState(158);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 12583038) != 0)) {
          setState(157);
          paramList();
        }
        setState(160);
        match(CParser::RPAREN);
        setState(161);
        match(CParser::SEMI);
        break;
      }

      case CParser::INT_KW:
      case CParser::CHAR_KW:
      case CParser::SHORT_KW:
      case CParser::LONG_KW:
      case CParser::SIGNED_KW:
      case CParser::UNSIGNED_KW:
      case CParser::VOID_KW:
      case CParser::STATIC_KW:
      case CParser::STRUCT_KW:
      case CParser::ENUM_KW: {
        _localctx = _tracker.createInstance<CParser::LocalFunctionDeclContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(164);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::STATIC_KW) {
          setState(163);
          antlrcpp::downCast<LocalFunctionDeclContext *>(_localctx)->stat = match(CParser::STATIC_KW);
        }
        setState(166);
        returnTypeSpec();
        setState(167);
        match(CParser::IDENT);
        setState(168);
        match(CParser::LPAREN);
        setState(170);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 12583038) != 0)) {
          setState(169);
          paramList();
        }
        setState(172);
        match(CParser::RPAREN);
        setState(175);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::AT) {
          setState(173);
          match(CParser::AT);
          setState(174);
          antlrcpp::downCast<LocalFunctionDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
        }
        setState(179);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case CParser::LBRACE: {
            setState(177);
            block();
            break;
          }

          case CParser::SEMI: {
            setState(178);
            match(CParser::SEMI);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ReturnTypeSpecContext ------------------------------------------------------------------

CParser::ReturnTypeSpecContext::ReturnTypeSpecContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

CParser::TypeSpecContext* CParser::ReturnTypeSpecContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::ReturnTypeSpecContext::VOID_KW() {
  return getToken(CParser::VOID_KW, 0);
}


size_t CParser::ReturnTypeSpecContext::getRuleIndex() const {
  return CParser::RuleReturnTypeSpec;
}


std::any CParser::ReturnTypeSpecContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitReturnTypeSpec(this);
  else
    return visitor->visitChildren(this);
}

CParser::ReturnTypeSpecContext* CParser::returnTypeSpec() {
  ReturnTypeSpecContext *_localctx = _tracker.createInstance<ReturnTypeSpecContext>(_ctx, getState());
  enterRule(_localctx, 18, CParser::RuleReturnTypeSpec);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(185);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case CParser::INT_KW:
      case CParser::CHAR_KW:
      case CParser::SHORT_KW:
      case CParser::LONG_KW:
      case CParser::SIGNED_KW:
      case CParser::UNSIGNED_KW:
      case CParser::STRUCT_KW:
      case CParser::ENUM_KW: {
        enterOuterAlt(_localctx, 1);
        setState(183);
        typeSpec();
        break;
      }

      case CParser::VOID_KW: {
        enterOuterAlt(_localctx, 2);
        setState(184);
        match(CParser::VOID_KW);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ParamListContext ------------------------------------------------------------------

CParser::ParamListContext::ParamListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<CParser::ParamContext *> CParser::ParamListContext::param() {
  return getRuleContexts<CParser::ParamContext>();
}

CParser::ParamContext* CParser::ParamListContext::param(size_t i) {
  return getRuleContext<CParser::ParamContext>(i);
}

std::vector<tree::TerminalNode *> CParser::ParamListContext::COMMA() {
  return getTokens(CParser::COMMA);
}

tree::TerminalNode* CParser::ParamListContext::COMMA(size_t i) {
  return getToken(CParser::COMMA, i);
}


size_t CParser::ParamListContext::getRuleIndex() const {
  return CParser::RuleParamList;
}


std::any CParser::ParamListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitParamList(this);
  else
    return visitor->visitChildren(this);
}

CParser::ParamListContext* CParser::paramList() {
  ParamListContext *_localctx = _tracker.createInstance<ParamListContext>(_ctx, getState());
  enterRule(_localctx, 20, CParser::RuleParamList);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(187);
    param();
    setState(192);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(188);
      match(CParser::COMMA);
      setState(189);
      param();
      setState(194);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ParamContext ------------------------------------------------------------------

CParser::ParamContext::ParamContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

CParser::TypeSpecContext* CParser::ParamContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::ParamContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}


size_t CParser::ParamContext::getRuleIndex() const {
  return CParser::RuleParam;
}


std::any CParser::ParamContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitParam(this);
  else
    return visitor->visitChildren(this);
}

CParser::ParamContext* CParser::param() {
  ParamContext *_localctx = _tracker.createInstance<ParamContext>(_ctx, getState());
  enterRule(_localctx, 22, CParser::RuleParam);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(195);
    typeSpec();
    setState(196);
    match(CParser::IDENT);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- GlobalDeclContext ------------------------------------------------------------------

CParser::GlobalDeclContext::GlobalDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::GlobalDeclContext::getRuleIndex() const {
  return CParser::RuleGlobalDecl;
}

void CParser::GlobalDeclContext::copyFrom(GlobalDeclContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- ExternScalarDeclContext ------------------------------------------------------------------

tree::TerminalNode* CParser::ExternScalarDeclContext::EXTERN_KW() {
  return getToken(CParser::EXTERN_KW, 0);
}

CParser::TypeSpecContext* CParser::ExternScalarDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::ExternScalarDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::ExternScalarDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::ExternScalarDeclContext::ExternScalarDeclContext(GlobalDeclContext *ctx) { copyFrom(ctx); }


std::any CParser::ExternScalarDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitExternScalarDecl(this);
  else
    return visitor->visitChildren(this);
}
//----------------- GlobalArrayDeclContext ------------------------------------------------------------------

CParser::TypeSpecContext* CParser::GlobalArrayDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::GlobalArrayDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::GlobalArrayDeclContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::GlobalArrayDeclContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

tree::TerminalNode* CParser::GlobalArrayDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

std::vector<tree::TerminalNode *> CParser::GlobalArrayDeclContext::NUMBER() {
  return getTokens(CParser::NUMBER);
}

tree::TerminalNode* CParser::GlobalArrayDeclContext::NUMBER(size_t i) {
  return getToken(CParser::NUMBER, i);
}

tree::TerminalNode* CParser::GlobalArrayDeclContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

CParser::ArrayInitContext* CParser::GlobalArrayDeclContext::arrayInit() {
  return getRuleContext<CParser::ArrayInitContext>(0);
}

tree::TerminalNode* CParser::GlobalArrayDeclContext::AT() {
  return getToken(CParser::AT, 0);
}

tree::TerminalNode* CParser::GlobalArrayDeclContext::STATIC_KW() {
  return getToken(CParser::STATIC_KW, 0);
}

CParser::GlobalArrayDeclContext::GlobalArrayDeclContext(GlobalDeclContext *ctx) { copyFrom(ctx); }


std::any CParser::GlobalArrayDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitGlobalArrayDecl(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ExternArrayDeclContext ------------------------------------------------------------------

tree::TerminalNode* CParser::ExternArrayDeclContext::EXTERN_KW() {
  return getToken(CParser::EXTERN_KW, 0);
}

CParser::TypeSpecContext* CParser::ExternArrayDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::ExternArrayDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::ExternArrayDeclContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::ExternArrayDeclContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

tree::TerminalNode* CParser::ExternArrayDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

tree::TerminalNode* CParser::ExternArrayDeclContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

CParser::ExternArrayDeclContext::ExternArrayDeclContext(GlobalDeclContext *ctx) { copyFrom(ctx); }


std::any CParser::ExternArrayDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitExternArrayDecl(this);
  else
    return visitor->visitChildren(this);
}
//----------------- GlobalScalarDeclContext ------------------------------------------------------------------

CParser::TypeSpecContext* CParser::GlobalScalarDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

std::vector<tree::TerminalNode *> CParser::GlobalScalarDeclContext::IDENT() {
  return getTokens(CParser::IDENT);
}

tree::TerminalNode* CParser::GlobalScalarDeclContext::IDENT(size_t i) {
  return getToken(CParser::IDENT, i);
}

tree::TerminalNode* CParser::GlobalScalarDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

tree::TerminalNode* CParser::GlobalScalarDeclContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

tree::TerminalNode* CParser::GlobalScalarDeclContext::AT() {
  return getToken(CParser::AT, 0);
}

tree::TerminalNode* CParser::GlobalScalarDeclContext::STATIC_KW() {
  return getToken(CParser::STATIC_KW, 0);
}

std::vector<tree::TerminalNode *> CParser::GlobalScalarDeclContext::NUMBER() {
  return getTokens(CParser::NUMBER);
}

tree::TerminalNode* CParser::GlobalScalarDeclContext::NUMBER(size_t i) {
  return getToken(CParser::NUMBER, i);
}

tree::TerminalNode* CParser::GlobalScalarDeclContext::MINUS() {
  return getToken(CParser::MINUS, 0);
}

CParser::GlobalScalarDeclContext::GlobalScalarDeclContext(GlobalDeclContext *ctx) { copyFrom(ctx); }


std::any CParser::GlobalScalarDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitGlobalScalarDecl(this);
  else
    return visitor->visitChildren(this);
}
CParser::GlobalDeclContext* CParser::globalDecl() {
  GlobalDeclContext *_localctx = _tracker.createInstance<GlobalDeclContext>(_ctx, getState());
  enterRule(_localctx, 24, CParser::RuleGlobalDecl);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(250);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 35, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::ExternArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(198);
      match(CParser::EXTERN_KW);
      setState(199);
      typeSpec();
      setState(200);
      match(CParser::IDENT);
      setState(201);
      match(CParser::LBRACK);
      setState(202);
      antlrcpp::downCast<ExternArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(203);
      match(CParser::RBRACK);
      setState(204);
      match(CParser::SEMI);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::ExternScalarDeclContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(206);
      match(CParser::EXTERN_KW);
      setState(207);
      typeSpec();
      setState(208);
      match(CParser::IDENT);
      setState(209);
      match(CParser::SEMI);
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::GlobalArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(212);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::STATIC_KW) {
        setState(211);
        antlrcpp::downCast<GlobalArrayDeclContext *>(_localctx)->stat = match(CParser::STATIC_KW);
      }
      setState(214);
      typeSpec();
      setState(215);
      match(CParser::IDENT);
      setState(216);
      match(CParser::LBRACK);
      setState(217);
      antlrcpp::downCast<GlobalArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(218);
      match(CParser::RBRACK);
      setState(221);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(219);
        match(CParser::ASSIGN);
        setState(220);
        arrayInit();
      }
      setState(225);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::AT) {
        setState(223);
        match(CParser::AT);
        setState(224);
        antlrcpp::downCast<GlobalArrayDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
      }
      setState(227);
      match(CParser::SEMI);
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::GlobalScalarDeclContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(230);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::STATIC_KW) {
        setState(229);
        antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->stat = match(CParser::STATIC_KW);
      }
      setState(232);
      typeSpec();
      setState(233);
      match(CParser::IDENT);
      setState(242);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(234);
        match(CParser::ASSIGN);
        setState(240);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case CParser::MINUS:
          case CParser::NUMBER: {
            setState(236);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == CParser::MINUS) {
              setState(235);
              antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->sign = match(CParser::MINUS);
            }
            setState(238);
            antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->val = match(CParser::NUMBER);
            break;
          }

          case CParser::IDENT: {
            setState(239);
            antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->identVal = match(CParser::IDENT);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
      }
      setState(246);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::AT) {
        setState(244);
        match(CParser::AT);
        setState(245);
        antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
      }
      setState(248);
      match(CParser::SEMI);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ArrayInitContext ------------------------------------------------------------------

CParser::ArrayInitContext::ArrayInitContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* CParser::ArrayInitContext::LBRACE() {
  return getToken(CParser::LBRACE, 0);
}

tree::TerminalNode* CParser::ArrayInitContext::RBRACE() {
  return getToken(CParser::RBRACE, 0);
}

std::vector<CParser::ArrayInitElemContext *> CParser::ArrayInitContext::arrayInitElem() {
  return getRuleContexts<CParser::ArrayInitElemContext>();
}

CParser::ArrayInitElemContext* CParser::ArrayInitContext::arrayInitElem(size_t i) {
  return getRuleContext<CParser::ArrayInitElemContext>(i);
}

std::vector<tree::TerminalNode *> CParser::ArrayInitContext::COMMA() {
  return getTokens(CParser::COMMA);
}

tree::TerminalNode* CParser::ArrayInitContext::COMMA(size_t i) {
  return getToken(CParser::COMMA, i);
}


size_t CParser::ArrayInitContext::getRuleIndex() const {
  return CParser::RuleArrayInit;
}


std::any CParser::ArrayInitContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitArrayInit(this);
  else
    return visitor->visitChildren(this);
}

CParser::ArrayInitContext* CParser::arrayInit() {
  ArrayInitContext *_localctx = _tracker.createInstance<ArrayInitContext>(_ctx, getState());
  enterRule(_localctx, 26, CParser::RuleArrayInit);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(252);
    match(CParser::LBRACE);
    setState(253);
    antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext = arrayInitElem();
    antlrcpp::downCast<ArrayInitContext *>(_localctx)->elem.push_back(antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext);
    setState(258);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(254);
      match(CParser::COMMA);
      setState(255);
      antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext = arrayInitElem();
      antlrcpp::downCast<ArrayInitContext *>(_localctx)->elem.push_back(antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext);
      setState(260);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(261);
    match(CParser::RBRACE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ArrayInitElemContext ------------------------------------------------------------------

CParser::ArrayInitElemContext::ArrayInitElemContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::ArrayInitElemContext::getRuleIndex() const {
  return CParser::RuleArrayInitElem;
}

void CParser::ArrayInitElemContext::copyFrom(ArrayInitElemContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- SymArrayElemContext ------------------------------------------------------------------

tree::TerminalNode* CParser::SymArrayElemContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

CParser::SymArrayElemContext::SymArrayElemContext(ArrayInitElemContext *ctx) { copyFrom(ctx); }


std::any CParser::SymArrayElemContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitSymArrayElem(this);
  else
    return visitor->visitChildren(this);
}
//----------------- NumArrayElemContext ------------------------------------------------------------------

tree::TerminalNode* CParser::NumArrayElemContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

tree::TerminalNode* CParser::NumArrayElemContext::MINUS() {
  return getToken(CParser::MINUS, 0);
}

CParser::NumArrayElemContext::NumArrayElemContext(ArrayInitElemContext *ctx) { copyFrom(ctx); }


std::any CParser::NumArrayElemContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitNumArrayElem(this);
  else
    return visitor->visitChildren(this);
}
CParser::ArrayInitElemContext* CParser::arrayInitElem() {
  ArrayInitElemContext *_localctx = _tracker.createInstance<ArrayInitElemContext>(_ctx, getState());
  enterRule(_localctx, 28, CParser::RuleArrayInitElem);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(268);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case CParser::MINUS:
      case CParser::NUMBER: {
        _localctx = _tracker.createInstance<CParser::NumArrayElemContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(264);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::MINUS) {
          setState(263);
          antlrcpp::downCast<NumArrayElemContext *>(_localctx)->sign = match(CParser::MINUS);
        }
        setState(266);
        match(CParser::NUMBER);
        break;
      }

      case CParser::IDENT: {
        _localctx = _tracker.createInstance<CParser::SymArrayElemContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(267);
        match(CParser::IDENT);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BlockContext ------------------------------------------------------------------

CParser::BlockContext::BlockContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* CParser::BlockContext::LBRACE() {
  return getToken(CParser::LBRACE, 0);
}

tree::TerminalNode* CParser::BlockContext::RBRACE() {
  return getToken(CParser::RBRACE, 0);
}

std::vector<CParser::StatementContext *> CParser::BlockContext::statement() {
  return getRuleContexts<CParser::StatementContext>();
}

CParser::StatementContext* CParser::BlockContext::statement(size_t i) {
  return getRuleContext<CParser::StatementContext>(i);
}


size_t CParser::BlockContext::getRuleIndex() const {
  return CParser::RuleBlock;
}


std::any CParser::BlockContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitBlock(this);
  else
    return visitor->visitChildren(this);
}

CParser::BlockContext* CParser::block() {
  BlockContext *_localctx = _tracker.createInstance<BlockContext>(_ctx, getState());
  enterRule(_localctx, 30, CParser::RuleBlock);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(270);
    match(CParser::LBRACE);
    setState(274);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 535024319358) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 9155) != 0)) {
      setState(271);
      statement();
      setState(276);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(277);
    match(CParser::RBRACE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StatementContext ------------------------------------------------------------------

CParser::StatementContext::StatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::StatementContext::getRuleIndex() const {
  return CParser::RuleStatement;
}

void CParser::StatementContext::copyFrom(StatementContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- ForStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::ForStmtContext::FOR_KW() {
  return getToken(CParser::FOR_KW, 0);
}

tree::TerminalNode* CParser::ForStmtContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

std::vector<tree::TerminalNode *> CParser::ForStmtContext::SEMI() {
  return getTokens(CParser::SEMI);
}

tree::TerminalNode* CParser::ForStmtContext::SEMI(size_t i) {
  return getToken(CParser::SEMI, i);
}

tree::TerminalNode* CParser::ForStmtContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::StatementContext* CParser::ForStmtContext::statement() {
  return getRuleContext<CParser::StatementContext>(0);
}

std::vector<CParser::SimpleStmtContext *> CParser::ForStmtContext::simpleStmt() {
  return getRuleContexts<CParser::SimpleStmtContext>();
}

CParser::SimpleStmtContext* CParser::ForStmtContext::simpleStmt(size_t i) {
  return getRuleContext<CParser::SimpleStmtContext>(i);
}

CParser::ExprContext* CParser::ForStmtContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::ForStmtContext::ForStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::ForStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitForStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AsmStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::AsmStmtContext::ASM_BEGIN() {
  return getToken(CParser::ASM_BEGIN, 0);
}

tree::TerminalNode* CParser::AsmStmtContext::ASM_END() {
  return getToken(CParser::ASM_END, 0);
}

tree::TerminalNode* CParser::AsmStmtContext::ASM_TEXT() {
  return getToken(CParser::ASM_TEXT, 0);
}

CParser::AsmStmtContext::AsmStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::AsmStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAsmStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- WhileStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::WhileStmtContext::WHILE_KW() {
  return getToken(CParser::WHILE_KW, 0);
}

tree::TerminalNode* CParser::WhileStmtContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

CParser::ExprContext* CParser::WhileStmtContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

tree::TerminalNode* CParser::WhileStmtContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::StatementContext* CParser::WhileStmtContext::statement() {
  return getRuleContext<CParser::StatementContext>(0);
}

CParser::WhileStmtContext::WhileStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::WhileStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitWhileStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- SimpleStmtWrapperContext ------------------------------------------------------------------

CParser::SimpleStmtContext* CParser::SimpleStmtWrapperContext::simpleStmt() {
  return getRuleContext<CParser::SimpleStmtContext>(0);
}

tree::TerminalNode* CParser::SimpleStmtWrapperContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::SimpleStmtWrapperContext::SimpleStmtWrapperContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::SimpleStmtWrapperContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitSimpleStmtWrapper(this);
  else
    return visitor->visitChildren(this);
}
//----------------- GotoStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::GotoStmtContext::GOTO_KW() {
  return getToken(CParser::GOTO_KW, 0);
}

tree::TerminalNode* CParser::GotoStmtContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::GotoStmtContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::GotoStmtContext::GotoStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::GotoStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitGotoStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- EmptyStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::EmptyStmtContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::EmptyStmtContext::EmptyStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::EmptyStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitEmptyStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ReturnStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::ReturnStmtContext::RETURN_KW() {
  return getToken(CParser::RETURN_KW, 0);
}

tree::TerminalNode* CParser::ReturnStmtContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::ExprContext* CParser::ReturnStmtContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::ReturnStmtContext::ReturnStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::ReturnStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitReturnStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- SwitchStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::SwitchStmtContext::SWITCH_KW() {
  return getToken(CParser::SWITCH_KW, 0);
}

tree::TerminalNode* CParser::SwitchStmtContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

CParser::ExprContext* CParser::SwitchStmtContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

tree::TerminalNode* CParser::SwitchStmtContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

tree::TerminalNode* CParser::SwitchStmtContext::LBRACE() {
  return getToken(CParser::LBRACE, 0);
}

tree::TerminalNode* CParser::SwitchStmtContext::RBRACE() {
  return getToken(CParser::RBRACE, 0);
}

std::vector<CParser::SwitchCaseContext *> CParser::SwitchStmtContext::switchCase() {
  return getRuleContexts<CParser::SwitchCaseContext>();
}

CParser::SwitchCaseContext* CParser::SwitchStmtContext::switchCase(size_t i) {
  return getRuleContext<CParser::SwitchCaseContext>(i);
}

CParser::SwitchStmtContext::SwitchStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::SwitchStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitSwitchStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IfStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::IfStmtContext::IF_KW() {
  return getToken(CParser::IF_KW, 0);
}

tree::TerminalNode* CParser::IfStmtContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

CParser::ExprContext* CParser::IfStmtContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

tree::TerminalNode* CParser::IfStmtContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

std::vector<CParser::StatementContext *> CParser::IfStmtContext::statement() {
  return getRuleContexts<CParser::StatementContext>();
}

CParser::StatementContext* CParser::IfStmtContext::statement(size_t i) {
  return getRuleContext<CParser::StatementContext>(i);
}

tree::TerminalNode* CParser::IfStmtContext::ELSE_KW() {
  return getToken(CParser::ELSE_KW, 0);
}

CParser::IfStmtContext::IfStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::IfStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitIfStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BlockStmtContext ------------------------------------------------------------------

CParser::BlockContext* CParser::BlockStmtContext::block() {
  return getRuleContext<CParser::BlockContext>(0);
}

CParser::BlockStmtContext::BlockStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::BlockStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitBlockStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BreakStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::BreakStmtContext::BREAK_KW() {
  return getToken(CParser::BREAK_KW, 0);
}

tree::TerminalNode* CParser::BreakStmtContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::BreakStmtContext::BreakStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::BreakStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitBreakStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LabelStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::LabelStmtContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::LabelStmtContext::COLON() {
  return getToken(CParser::COLON, 0);
}

CParser::StatementContext* CParser::LabelStmtContext::statement() {
  return getRuleContext<CParser::StatementContext>(0);
}

CParser::LabelStmtContext::LabelStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::LabelStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitLabelStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ContinueStmtContext ------------------------------------------------------------------

tree::TerminalNode* CParser::ContinueStmtContext::CONTINUE_KW() {
  return getToken(CParser::CONTINUE_KW, 0);
}

tree::TerminalNode* CParser::ContinueStmtContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::ContinueStmtContext::ContinueStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any CParser::ContinueStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitContinueStmt(this);
  else
    return visitor->visitChildren(this);
}
CParser::StatementContext* CParser::statement() {
  StatementContext *_localctx = _tracker.createInstance<StatementContext>(_ctx, getState());
  enterRule(_localctx, 32, CParser::RuleStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(347);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 47, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::BlockStmtContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(279);
      block();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::IfStmtContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(280);
      match(CParser::IF_KW);
      setState(281);
      match(CParser::LPAREN);
      setState(282);
      expr();
      setState(283);
      match(CParser::RPAREN);
      setState(284);
      antlrcpp::downCast<IfStmtContext *>(_localctx)->thenS = statement();
      setState(287);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 40, _ctx)) {
      case 1: {
        setState(285);
        match(CParser::ELSE_KW);
        setState(286);
        antlrcpp::downCast<IfStmtContext *>(_localctx)->elseS = statement();
        break;
      }

      default:
        break;
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::WhileStmtContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(289);
      match(CParser::WHILE_KW);
      setState(290);
      match(CParser::LPAREN);
      setState(291);
      expr();
      setState(292);
      match(CParser::RPAREN);
      setState(293);
      statement();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::ForStmtContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(295);
      match(CParser::FOR_KW);
      setState(296);
      match(CParser::LPAREN);
      setState(298);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 532607402366) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 963) != 0)) {
        setState(297);
        antlrcpp::downCast<ForStmtContext *>(_localctx)->init = simpleStmt();
      }
      setState(300);
      match(CParser::SEMI);
      setState(302);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 532592722176) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 963) != 0)) {
        setState(301);
        antlrcpp::downCast<ForStmtContext *>(_localctx)->cond = expr();
      }
      setState(304);
      match(CParser::SEMI);
      setState(306);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 532607402366) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 963) != 0)) {
        setState(305);
        antlrcpp::downCast<ForStmtContext *>(_localctx)->post = simpleStmt();
      }
      setState(308);
      match(CParser::RPAREN);
      setState(309);
      statement();
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<CParser::SwitchStmtContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(310);
      match(CParser::SWITCH_KW);
      setState(311);
      match(CParser::LPAREN);
      setState(312);
      expr();
      setState(313);
      match(CParser::RPAREN);
      setState(314);
      match(CParser::LBRACE);
      setState(318);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == CParser::CASE_KW

      || _la == CParser::DEFAULT_KW) {
        setState(315);
        switchCase();
        setState(320);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
      setState(321);
      match(CParser::RBRACE);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<CParser::ReturnStmtContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(323);
      match(CParser::RETURN_KW);
      setState(325);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 532592722176) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 963) != 0)) {
        setState(324);
        expr();
      }
      setState(327);
      match(CParser::SEMI);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<CParser::BreakStmtContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(328);
      match(CParser::BREAK_KW);
      setState(329);
      match(CParser::SEMI);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<CParser::ContinueStmtContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(330);
      match(CParser::CONTINUE_KW);
      setState(331);
      match(CParser::SEMI);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<CParser::GotoStmtContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(332);
      match(CParser::GOTO_KW);
      setState(333);
      match(CParser::IDENT);
      setState(334);
      match(CParser::SEMI);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<CParser::LabelStmtContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(335);
      match(CParser::IDENT);
      setState(336);
      match(CParser::COLON);
      setState(337);
      statement();
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<CParser::AsmStmtContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(338);
      match(CParser::ASM_BEGIN);
      setState(340);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASM_TEXT) {
        setState(339);
        antlrcpp::downCast<AsmStmtContext *>(_localctx)->text = match(CParser::ASM_TEXT);
      }
      setState(342);
      match(CParser::ASM_END);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<CParser::SimpleStmtWrapperContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(343);
      simpleStmt();
      setState(344);
      match(CParser::SEMI);
      break;
    }

    case 13: {
      _localctx = _tracker.createInstance<CParser::EmptyStmtContext>(_localctx);
      enterOuterAlt(_localctx, 13);
      setState(346);
      match(CParser::SEMI);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SimpleStmtContext ------------------------------------------------------------------

CParser::SimpleStmtContext::SimpleStmtContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::SimpleStmtContext::getRuleIndex() const {
  return CParser::RuleSimpleStmt;
}

void CParser::SimpleStmtContext::copyFrom(SimpleStmtContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- ExprStmtContext ------------------------------------------------------------------

CParser::ExprContext* CParser::ExprStmtContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::ExprStmtContext::ExprStmtContext(SimpleStmtContext *ctx) { copyFrom(ctx); }


std::any CParser::ExprStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitExprStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LocalDeclContext ------------------------------------------------------------------

CParser::TypeSpecContext* CParser::LocalDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::LocalDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::LocalDeclContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

CParser::ExprContext* CParser::LocalDeclContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::LocalDeclContext::LocalDeclContext(SimpleStmtContext *ctx) { copyFrom(ctx); }


std::any CParser::LocalDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitLocalDecl(this);
  else
    return visitor->visitChildren(this);
}
//----------------- StaticLocalArrayDeclContext ------------------------------------------------------------------

tree::TerminalNode* CParser::StaticLocalArrayDeclContext::STATIC_KW() {
  return getToken(CParser::STATIC_KW, 0);
}

CParser::TypeSpecContext* CParser::StaticLocalArrayDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::StaticLocalArrayDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::StaticLocalArrayDeclContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::StaticLocalArrayDeclContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

tree::TerminalNode* CParser::StaticLocalArrayDeclContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

tree::TerminalNode* CParser::StaticLocalArrayDeclContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

CParser::ArrayInitContext* CParser::StaticLocalArrayDeclContext::arrayInit() {
  return getRuleContext<CParser::ArrayInitContext>(0);
}

CParser::StaticLocalArrayDeclContext::StaticLocalArrayDeclContext(SimpleStmtContext *ctx) { copyFrom(ctx); }


std::any CParser::StaticLocalArrayDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitStaticLocalArrayDecl(this);
  else
    return visitor->visitChildren(this);
}
//----------------- StaticLocalDeclContext ------------------------------------------------------------------

tree::TerminalNode* CParser::StaticLocalDeclContext::STATIC_KW() {
  return getToken(CParser::STATIC_KW, 0);
}

CParser::TypeSpecContext* CParser::StaticLocalDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

std::vector<tree::TerminalNode *> CParser::StaticLocalDeclContext::IDENT() {
  return getTokens(CParser::IDENT);
}

tree::TerminalNode* CParser::StaticLocalDeclContext::IDENT(size_t i) {
  return getToken(CParser::IDENT, i);
}

tree::TerminalNode* CParser::StaticLocalDeclContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

tree::TerminalNode* CParser::StaticLocalDeclContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

tree::TerminalNode* CParser::StaticLocalDeclContext::MINUS() {
  return getToken(CParser::MINUS, 0);
}

CParser::StaticLocalDeclContext::StaticLocalDeclContext(SimpleStmtContext *ctx) { copyFrom(ctx); }


std::any CParser::StaticLocalDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitStaticLocalDecl(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LocalArrayDeclContext ------------------------------------------------------------------

CParser::TypeSpecContext* CParser::LocalArrayDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::LocalArrayDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::LocalArrayDeclContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::LocalArrayDeclContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

tree::TerminalNode* CParser::LocalArrayDeclContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

tree::TerminalNode* CParser::LocalArrayDeclContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

CParser::ArrayInitContext* CParser::LocalArrayDeclContext::arrayInit() {
  return getRuleContext<CParser::ArrayInitContext>(0);
}

CParser::LocalArrayDeclContext::LocalArrayDeclContext(SimpleStmtContext *ctx) { copyFrom(ctx); }


std::any CParser::LocalArrayDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitLocalArrayDecl(this);
  else
    return visitor->visitChildren(this);
}
CParser::SimpleStmtContext* CParser::simpleStmt() {
  SimpleStmtContext *_localctx = _tracker.createInstance<SimpleStmtContext>(_ctx, getState());
  enterRule(_localctx, 34, CParser::RuleSimpleStmt);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(388);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 54, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::LocalArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(349);
      typeSpec();
      setState(350);
      match(CParser::IDENT);
      setState(351);
      match(CParser::LBRACK);
      setState(352);
      antlrcpp::downCast<LocalArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(353);
      match(CParser::RBRACK);
      setState(356);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(354);
        match(CParser::ASSIGN);
        setState(355);
        arrayInit();
      }
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::LocalDeclContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(358);
      typeSpec();
      setState(359);
      match(CParser::IDENT);
      setState(362);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(360);
        match(CParser::ASSIGN);
        setState(361);
        expr();
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::StaticLocalArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(364);
      match(CParser::STATIC_KW);
      setState(365);
      typeSpec();
      setState(366);
      match(CParser::IDENT);
      setState(367);
      match(CParser::LBRACK);
      setState(368);
      antlrcpp::downCast<StaticLocalArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(369);
      match(CParser::RBRACK);
      setState(372);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(370);
        match(CParser::ASSIGN);
        setState(371);
        arrayInit();
      }
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::StaticLocalDeclContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(374);
      match(CParser::STATIC_KW);
      setState(375);
      typeSpec();
      setState(376);
      match(CParser::IDENT);
      setState(385);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(377);
        match(CParser::ASSIGN);
        setState(383);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case CParser::MINUS:
          case CParser::NUMBER: {
            setState(379);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == CParser::MINUS) {
              setState(378);
              antlrcpp::downCast<StaticLocalDeclContext *>(_localctx)->sign = match(CParser::MINUS);
            }
            setState(381);
            antlrcpp::downCast<StaticLocalDeclContext *>(_localctx)->val = match(CParser::NUMBER);
            break;
          }

          case CParser::IDENT: {
            setState(382);
            antlrcpp::downCast<StaticLocalDeclContext *>(_localctx)->identVal = match(CParser::IDENT);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
      }
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<CParser::ExprStmtContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(387);
      expr();
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SwitchCaseContext ------------------------------------------------------------------

CParser::SwitchCaseContext::SwitchCaseContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::SwitchCaseContext::getRuleIndex() const {
  return CParser::RuleSwitchCase;
}

void CParser::SwitchCaseContext::copyFrom(SwitchCaseContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- DefaultLabelContext ------------------------------------------------------------------

tree::TerminalNode* CParser::DefaultLabelContext::DEFAULT_KW() {
  return getToken(CParser::DEFAULT_KW, 0);
}

tree::TerminalNode* CParser::DefaultLabelContext::COLON() {
  return getToken(CParser::COLON, 0);
}

std::vector<CParser::StatementContext *> CParser::DefaultLabelContext::statement() {
  return getRuleContexts<CParser::StatementContext>();
}

CParser::StatementContext* CParser::DefaultLabelContext::statement(size_t i) {
  return getRuleContext<CParser::StatementContext>(i);
}

CParser::DefaultLabelContext::DefaultLabelContext(SwitchCaseContext *ctx) { copyFrom(ctx); }


std::any CParser::DefaultLabelContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitDefaultLabel(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CaseLabelContext ------------------------------------------------------------------

tree::TerminalNode* CParser::CaseLabelContext::CASE_KW() {
  return getToken(CParser::CASE_KW, 0);
}

tree::TerminalNode* CParser::CaseLabelContext::COLON() {
  return getToken(CParser::COLON, 0);
}

tree::TerminalNode* CParser::CaseLabelContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

tree::TerminalNode* CParser::CaseLabelContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

std::vector<CParser::StatementContext *> CParser::CaseLabelContext::statement() {
  return getRuleContexts<CParser::StatementContext>();
}

CParser::StatementContext* CParser::CaseLabelContext::statement(size_t i) {
  return getRuleContext<CParser::StatementContext>(i);
}

tree::TerminalNode* CParser::CaseLabelContext::MINUS() {
  return getToken(CParser::MINUS, 0);
}

CParser::CaseLabelContext::CaseLabelContext(SwitchCaseContext *ctx) { copyFrom(ctx); }


std::any CParser::CaseLabelContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitCaseLabel(this);
  else
    return visitor->visitChildren(this);
}
CParser::SwitchCaseContext* CParser::switchCase() {
  SwitchCaseContext *_localctx = _tracker.createInstance<SwitchCaseContext>(_ctx, getState());
  enterRule(_localctx, 36, CParser::RuleSwitchCase);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(413);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case CParser::CASE_KW: {
        _localctx = _tracker.createInstance<CParser::CaseLabelContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(390);
        match(CParser::CASE_KW);
        setState(396);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case CParser::MINUS:
          case CParser::NUMBER: {
            setState(392);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == CParser::MINUS) {
              setState(391);
              antlrcpp::downCast<CaseLabelContext *>(_localctx)->sign = match(CParser::MINUS);
            }
            setState(394);
            antlrcpp::downCast<CaseLabelContext *>(_localctx)->val = match(CParser::NUMBER);
            break;
          }

          case CParser::IDENT: {
            setState(395);
            antlrcpp::downCast<CaseLabelContext *>(_localctx)->identVal = match(CParser::IDENT);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        setState(398);
        match(CParser::COLON);
        setState(402);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 535024319358) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & 9155) != 0)) {
          setState(399);
          statement();
          setState(404);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case CParser::DEFAULT_KW: {
        _localctx = _tracker.createInstance<CParser::DefaultLabelContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(405);
        match(CParser::DEFAULT_KW);
        setState(406);
        match(CParser::COLON);
        setState(410);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 535024319358) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & 9155) != 0)) {
          setState(407);
          statement();
          setState(412);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AssignOpContext ------------------------------------------------------------------

CParser::AssignOpContext::AssignOpContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* CParser::AssignOpContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

tree::TerminalNode* CParser::AssignOpContext::PLUSEQ() {
  return getToken(CParser::PLUSEQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::MINUSEQ() {
  return getToken(CParser::MINUSEQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::STAREQ() {
  return getToken(CParser::STAREQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::SLASHEQ() {
  return getToken(CParser::SLASHEQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::PERCENTEQ() {
  return getToken(CParser::PERCENTEQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::AMPEQ() {
  return getToken(CParser::AMPEQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::CARETEQ() {
  return getToken(CParser::CARETEQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::PIPEEQ() {
  return getToken(CParser::PIPEEQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::SHLEQ() {
  return getToken(CParser::SHLEQ, 0);
}

tree::TerminalNode* CParser::AssignOpContext::SHREQ() {
  return getToken(CParser::SHREQ, 0);
}


size_t CParser::AssignOpContext::getRuleIndex() const {
  return CParser::RuleAssignOp;
}


std::any CParser::AssignOpContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAssignOp(this);
  else
    return visitor->visitChildren(this);
}

CParser::AssignOpContext* CParser::assignOp() {
  AssignOpContext *_localctx = _tracker.createInstance<AssignOpContext>(_ctx, getState());
  enterRule(_localctx, 38, CParser::RuleAssignOp);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(415);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -18014389919547392) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExprContext ------------------------------------------------------------------

CParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::ExprContext::getRuleIndex() const {
  return CParser::RuleExpr;
}

void CParser::ExprContext::copyFrom(ExprContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- AssignMemberExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::AssignMemberExprContext::DOT() {
  return getToken(CParser::DOT, 0);
}

CParser::CondExprContext* CParser::AssignMemberExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

tree::TerminalNode* CParser::AssignMemberExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

CParser::AssignOpContext* CParser::AssignMemberExprContext::assignOp() {
  return getRuleContext<CParser::AssignOpContext>(0);
}

std::vector<CParser::ExprContext *> CParser::AssignMemberExprContext::expr() {
  return getRuleContexts<CParser::ExprContext>();
}

CParser::ExprContext* CParser::AssignMemberExprContext::expr(size_t i) {
  return getRuleContext<CParser::ExprContext>(i);
}

tree::TerminalNode* CParser::AssignMemberExprContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::AssignMemberExprContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

CParser::AssignMemberExprContext::AssignMemberExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any CParser::AssignMemberExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAssignMemberExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ExprCondContext ------------------------------------------------------------------

CParser::CondExprContext* CParser::ExprCondContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

CParser::ExprCondContext::ExprCondContext(ExprContext *ctx) { copyFrom(ctx); }


std::any CParser::ExprCondContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitExprCond(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AssignIndexExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::AssignIndexExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::AssignIndexExprContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::AssignIndexExprContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

std::vector<CParser::ExprContext *> CParser::AssignIndexExprContext::expr() {
  return getRuleContexts<CParser::ExprContext>();
}

CParser::ExprContext* CParser::AssignIndexExprContext::expr(size_t i) {
  return getRuleContext<CParser::ExprContext>(i);
}

CParser::AssignOpContext* CParser::AssignIndexExprContext::assignOp() {
  return getRuleContext<CParser::AssignOpContext>(0);
}

CParser::AssignIndexExprContext::AssignIndexExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any CParser::AssignIndexExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAssignIndexExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AssignMemberPtrExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::AssignMemberPtrExprContext::ARROW() {
  return getToken(CParser::ARROW, 0);
}

CParser::CondExprContext* CParser::AssignMemberPtrExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

tree::TerminalNode* CParser::AssignMemberPtrExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

CParser::AssignOpContext* CParser::AssignMemberPtrExprContext::assignOp() {
  return getRuleContext<CParser::AssignOpContext>(0);
}

std::vector<CParser::ExprContext *> CParser::AssignMemberPtrExprContext::expr() {
  return getRuleContexts<CParser::ExprContext>();
}

CParser::ExprContext* CParser::AssignMemberPtrExprContext::expr(size_t i) {
  return getRuleContext<CParser::ExprContext>(i);
}

tree::TerminalNode* CParser::AssignMemberPtrExprContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::AssignMemberPtrExprContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

CParser::AssignMemberPtrExprContext::AssignMemberPtrExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any CParser::AssignMemberPtrExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAssignMemberPtrExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AssignDerefExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::AssignDerefExprContext::STAR() {
  return getToken(CParser::STAR, 0);
}

CParser::CondExprContext* CParser::AssignDerefExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

CParser::AssignOpContext* CParser::AssignDerefExprContext::assignOp() {
  return getRuleContext<CParser::AssignOpContext>(0);
}

CParser::ExprContext* CParser::AssignDerefExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::AssignDerefExprContext::AssignDerefExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any CParser::AssignDerefExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAssignDerefExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AssignExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::AssignExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

CParser::AssignOpContext* CParser::AssignExprContext::assignOp() {
  return getRuleContext<CParser::AssignOpContext>(0);
}

CParser::ExprContext* CParser::AssignExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::AssignExprContext::AssignExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any CParser::AssignExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAssignExpr(this);
  else
    return visitor->visitChildren(this);
}
CParser::ExprContext* CParser::expr() {
  ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, getState());
  enterRule(_localctx, 40, CParser::RuleExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(458);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 62, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::AssignDerefExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(417);
      match(CParser::STAR);
      setState(418);
      antlrcpp::downCast<AssignDerefExprContext *>(_localctx)->ptr = condExpr(0);
      setState(419);
      antlrcpp::downCast<AssignDerefExprContext *>(_localctx)->op = assignOp();
      setState(420);
      antlrcpp::downCast<AssignDerefExprContext *>(_localctx)->val = expr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::AssignIndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(422);
      match(CParser::IDENT);
      setState(423);
      match(CParser::LBRACK);
      setState(424);
      antlrcpp::downCast<AssignIndexExprContext *>(_localctx)->idx = expr();
      setState(425);
      match(CParser::RBRACK);
      setState(426);
      antlrcpp::downCast<AssignIndexExprContext *>(_localctx)->op = assignOp();
      setState(427);
      antlrcpp::downCast<AssignIndexExprContext *>(_localctx)->val = expr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(429);
      match(CParser::IDENT);
      setState(430);
      antlrcpp::downCast<AssignExprContext *>(_localctx)->op = assignOp();
      setState(431);
      antlrcpp::downCast<AssignExprContext *>(_localctx)->val = expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::AssignMemberExprContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(433);
      antlrcpp::downCast<AssignMemberExprContext *>(_localctx)->base = condExpr(0);
      setState(434);
      match(CParser::DOT);
      setState(435);
      antlrcpp::downCast<AssignMemberExprContext *>(_localctx)->name = match(CParser::IDENT);
      setState(440);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::LBRACK) {
        setState(436);
        match(CParser::LBRACK);
        setState(437);
        antlrcpp::downCast<AssignMemberExprContext *>(_localctx)->idx = expr();
        setState(438);
        match(CParser::RBRACK);
      }
      setState(442);
      antlrcpp::downCast<AssignMemberExprContext *>(_localctx)->op = assignOp();
      setState(443);
      antlrcpp::downCast<AssignMemberExprContext *>(_localctx)->val = expr();
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<CParser::AssignMemberPtrExprContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(445);
      antlrcpp::downCast<AssignMemberPtrExprContext *>(_localctx)->base = condExpr(0);
      setState(446);
      match(CParser::ARROW);
      setState(447);
      antlrcpp::downCast<AssignMemberPtrExprContext *>(_localctx)->name = match(CParser::IDENT);
      setState(452);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::LBRACK) {
        setState(448);
        match(CParser::LBRACK);
        setState(449);
        antlrcpp::downCast<AssignMemberPtrExprContext *>(_localctx)->idx = expr();
        setState(450);
        match(CParser::RBRACK);
      }
      setState(454);
      antlrcpp::downCast<AssignMemberPtrExprContext *>(_localctx)->op = assignOp();
      setState(455);
      antlrcpp::downCast<AssignMemberPtrExprContext *>(_localctx)->val = expr();
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<CParser::ExprCondContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(457);
      condExpr(0);
      break;
    }

    default:
      break;
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CondExprContext ------------------------------------------------------------------

CParser::CondExprContext::CondExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t CParser::CondExprContext::getRuleIndex() const {
  return CParser::RuleCondExpr;
}

void CParser::CondExprContext::copyFrom(CondExprContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- SizeofExprExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::SizeofExprExprContext::SIZEOF_KW() {
  return getToken(CParser::SIZEOF_KW, 0);
}

tree::TerminalNode* CParser::SizeofExprExprContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

CParser::ExprContext* CParser::SizeofExprExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

tree::TerminalNode* CParser::SizeofExprExprContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::SizeofExprExprContext::SizeofExprExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::SizeofExprExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitSizeofExprExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- PreIncDecDerefExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::PreIncDecDerefExprContext::STAR() {
  return getToken(CParser::STAR, 0);
}

CParser::CondExprContext* CParser::PreIncDecDerefExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

tree::TerminalNode* CParser::PreIncDecDerefExprContext::INC() {
  return getToken(CParser::INC, 0);
}

tree::TerminalNode* CParser::PreIncDecDerefExprContext::DEC() {
  return getToken(CParser::DEC, 0);
}

CParser::PreIncDecDerefExprContext::PreIncDecDerefExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::PreIncDecDerefExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitPreIncDecDerefExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CastExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::CastExprContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

CParser::TypeSpecContext* CParser::CastExprContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::CastExprContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::CondExprContext* CParser::CastExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

CParser::CastExprContext::CastExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::CastExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitCastExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- PostIncDecVarExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::PostIncDecVarExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::PostIncDecVarExprContext::INC() {
  return getToken(CParser::INC, 0);
}

tree::TerminalNode* CParser::PostIncDecVarExprContext::DEC() {
  return getToken(CParser::DEC, 0);
}

CParser::PostIncDecVarExprContext::PostIncDecVarExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::PostIncDecVarExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitPostIncDecVarExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- StringLitExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::StringLitExprContext::STRINGLIT() {
  return getToken(CParser::STRINGLIT, 0);
}

CParser::StringLitExprContext::StringLitExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::StringLitExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitStringLitExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- PreIncDecIndexExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::PreIncDecIndexExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::PreIncDecIndexExprContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::PreIncDecIndexExprContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

CParser::ExprContext* CParser::PreIncDecIndexExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

tree::TerminalNode* CParser::PreIncDecIndexExprContext::INC() {
  return getToken(CParser::INC, 0);
}

tree::TerminalNode* CParser::PreIncDecIndexExprContext::DEC() {
  return getToken(CParser::DEC, 0);
}

CParser::PreIncDecIndexExprContext::PreIncDecIndexExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::PreIncDecIndexExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitPreIncDecIndexExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ParenExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::ParenExprContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

CParser::ExprContext* CParser::ParenExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

tree::TerminalNode* CParser::ParenExprContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::ParenExprContext::ParenExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::ParenExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitParenExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BitXorExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::BitXorExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::BitXorExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::BitXorExprContext::CARET() {
  return getToken(CParser::CARET, 0);
}

CParser::BitXorExprContext::BitXorExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::BitXorExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitBitXorExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- PostIncDecDerefExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::PostIncDecDerefExprContext::STAR() {
  return getToken(CParser::STAR, 0);
}

CParser::CondExprContext* CParser::PostIncDecDerefExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

tree::TerminalNode* CParser::PostIncDecDerefExprContext::INC() {
  return getToken(CParser::INC, 0);
}

tree::TerminalNode* CParser::PostIncDecDerefExprContext::DEC() {
  return getToken(CParser::DEC, 0);
}

CParser::PostIncDecDerefExprContext::PostIncDecDerefExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::PostIncDecDerefExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitPostIncDecDerefExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- UnaryExprContext ------------------------------------------------------------------

CParser::CondExprContext* CParser::UnaryExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

tree::TerminalNode* CParser::UnaryExprContext::MINUS() {
  return getToken(CParser::MINUS, 0);
}

tree::TerminalNode* CParser::UnaryExprContext::BANG() {
  return getToken(CParser::BANG, 0);
}

tree::TerminalNode* CParser::UnaryExprContext::TILDE() {
  return getToken(CParser::TILDE, 0);
}

CParser::UnaryExprContext::UnaryExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::UnaryExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitUnaryExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- PostIncDecIndexExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::PostIncDecIndexExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::PostIncDecIndexExprContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::PostIncDecIndexExprContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

CParser::ExprContext* CParser::PostIncDecIndexExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

tree::TerminalNode* CParser::PostIncDecIndexExprContext::INC() {
  return getToken(CParser::INC, 0);
}

tree::TerminalNode* CParser::PostIncDecIndexExprContext::DEC() {
  return getToken(CParser::DEC, 0);
}

CParser::PostIncDecIndexExprContext::PostIncDecIndexExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::PostIncDecIndexExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitPostIncDecIndexExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- TernaryExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::TernaryExprContext::QUESTION() {
  return getToken(CParser::QUESTION, 0);
}

tree::TerminalNode* CParser::TernaryExprContext::COLON() {
  return getToken(CParser::COLON, 0);
}

std::vector<CParser::CondExprContext *> CParser::TernaryExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::TernaryExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

CParser::ExprContext* CParser::TernaryExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::TernaryExprContext::TernaryExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::TernaryExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitTernaryExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- MulExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::MulExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::MulExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::MulExprContext::STAR() {
  return getToken(CParser::STAR, 0);
}

tree::TerminalNode* CParser::MulExprContext::SLASH() {
  return getToken(CParser::SLASH, 0);
}

tree::TerminalNode* CParser::MulExprContext::PERCENT() {
  return getToken(CParser::PERCENT, 0);
}

CParser::MulExprContext::MulExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::MulExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitMulExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- MemberAccessExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::MemberAccessExprContext::DOT() {
  return getToken(CParser::DOT, 0);
}

CParser::CondExprContext* CParser::MemberAccessExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

tree::TerminalNode* CParser::MemberAccessExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::MemberAccessExprContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::MemberAccessExprContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

CParser::ExprContext* CParser::MemberAccessExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::MemberAccessExprContext::MemberAccessExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::MemberAccessExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitMemberAccessExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CallExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::CallExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::CallExprContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

tree::TerminalNode* CParser::CallExprContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::ArgListContext* CParser::CallExprContext::argList() {
  return getRuleContext<CParser::ArgListContext>(0);
}

CParser::CallExprContext::CallExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::CallExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitCallExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- RelExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::RelExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::RelExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::RelExprContext::LT() {
  return getToken(CParser::LT, 0);
}

tree::TerminalNode* CParser::RelExprContext::LE() {
  return getToken(CParser::LE, 0);
}

tree::TerminalNode* CParser::RelExprContext::GT() {
  return getToken(CParser::GT, 0);
}

tree::TerminalNode* CParser::RelExprContext::GE() {
  return getToken(CParser::GE, 0);
}

CParser::RelExprContext::RelExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::RelExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitRelExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BitAndExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::BitAndExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::BitAndExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::BitAndExprContext::AMP() {
  return getToken(CParser::AMP, 0);
}

CParser::BitAndExprContext::BitAndExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::BitAndExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitBitAndExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CharExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::CharExprContext::CHARLIT() {
  return getToken(CParser::CHARLIT, 0);
}

CParser::CharExprContext::CharExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::CharExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitCharExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- SizeofTypeExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::SizeofTypeExprContext::SIZEOF_KW() {
  return getToken(CParser::SIZEOF_KW, 0);
}

tree::TerminalNode* CParser::SizeofTypeExprContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

CParser::TypeSpecContext* CParser::SizeofTypeExprContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
}

tree::TerminalNode* CParser::SizeofTypeExprContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::SizeofTypeExprContext::SizeofTypeExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::SizeofTypeExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitSizeofTypeExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AddrOfExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::AddrOfExprContext::AMP() {
  return getToken(CParser::AMP, 0);
}

CParser::CondExprContext* CParser::AddrOfExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

CParser::AddrOfExprContext::AddrOfExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::AddrOfExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAddrOfExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- OrExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::OrExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::OrExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::OrExprContext::OROR() {
  return getToken(CParser::OROR, 0);
}

CParser::OrExprContext::OrExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::OrExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitOrExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- MemberAccessPtrExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::MemberAccessPtrExprContext::ARROW() {
  return getToken(CParser::ARROW, 0);
}

CParser::CondExprContext* CParser::MemberAccessPtrExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

tree::TerminalNode* CParser::MemberAccessPtrExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::MemberAccessPtrExprContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

tree::TerminalNode* CParser::MemberAccessPtrExprContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

CParser::ExprContext* CParser::MemberAccessPtrExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

CParser::MemberAccessPtrExprContext::MemberAccessPtrExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::MemberAccessPtrExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitMemberAccessPtrExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CallPtrExprContext ------------------------------------------------------------------

std::vector<tree::TerminalNode *> CParser::CallPtrExprContext::LPAREN() {
  return getTokens(CParser::LPAREN);
}

tree::TerminalNode* CParser::CallPtrExprContext::LPAREN(size_t i) {
  return getToken(CParser::LPAREN, i);
}

CParser::ExprContext* CParser::CallPtrExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

std::vector<tree::TerminalNode *> CParser::CallPtrExprContext::RPAREN() {
  return getTokens(CParser::RPAREN);
}

tree::TerminalNode* CParser::CallPtrExprContext::RPAREN(size_t i) {
  return getToken(CParser::RPAREN, i);
}

CParser::ArgListContext* CParser::CallPtrExprContext::argList() {
  return getRuleContext<CParser::ArgListContext>(0);
}

CParser::CallPtrExprContext::CallPtrExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::CallPtrExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitCallPtrExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- NumExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::NumExprContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}

CParser::NumExprContext::NumExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::NumExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitNumExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ShiftExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::ShiftExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::ShiftExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::ShiftExprContext::SHL() {
  return getToken(CParser::SHL, 0);
}

tree::TerminalNode* CParser::ShiftExprContext::SHR() {
  return getToken(CParser::SHR, 0);
}

CParser::ShiftExprContext::ShiftExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::ShiftExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitShiftExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IndexExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::IndexExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::IndexExprContext::LBRACK() {
  return getToken(CParser::LBRACK, 0);
}

CParser::ExprContext* CParser::IndexExprContext::expr() {
  return getRuleContext<CParser::ExprContext>(0);
}

tree::TerminalNode* CParser::IndexExprContext::RBRACK() {
  return getToken(CParser::RBRACK, 0);
}

CParser::IndexExprContext::IndexExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::IndexExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitIndexExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- PreIncDecVarExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::PreIncDecVarExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::PreIncDecVarExprContext::INC() {
  return getToken(CParser::INC, 0);
}

tree::TerminalNode* CParser::PreIncDecVarExprContext::DEC() {
  return getToken(CParser::DEC, 0);
}

CParser::PreIncDecVarExprContext::PreIncDecVarExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::PreIncDecVarExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitPreIncDecVarExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- VarExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::VarExprContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

CParser::VarExprContext::VarExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::VarExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitVarExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- EqExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::EqExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::EqExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::EqExprContext::EQ() {
  return getToken(CParser::EQ, 0);
}

tree::TerminalNode* CParser::EqExprContext::NE() {
  return getToken(CParser::NE, 0);
}

CParser::EqExprContext::EqExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::EqExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitEqExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BitOrExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::BitOrExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::BitOrExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::BitOrExprContext::PIPE() {
  return getToken(CParser::PIPE, 0);
}

CParser::BitOrExprContext::BitOrExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::BitOrExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitBitOrExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AddExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::AddExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::AddExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::AddExprContext::PLUS() {
  return getToken(CParser::PLUS, 0);
}

tree::TerminalNode* CParser::AddExprContext::MINUS() {
  return getToken(CParser::MINUS, 0);
}

CParser::AddExprContext::AddExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::AddExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAddExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- DerefExprContext ------------------------------------------------------------------

tree::TerminalNode* CParser::DerefExprContext::STAR() {
  return getToken(CParser::STAR, 0);
}

CParser::CondExprContext* CParser::DerefExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
}

CParser::DerefExprContext::DerefExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::DerefExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitDerefExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AndExprContext ------------------------------------------------------------------

std::vector<CParser::CondExprContext *> CParser::AndExprContext::condExpr() {
  return getRuleContexts<CParser::CondExprContext>();
}

CParser::CondExprContext* CParser::AndExprContext::condExpr(size_t i) {
  return getRuleContext<CParser::CondExprContext>(i);
}

tree::TerminalNode* CParser::AndExprContext::ANDAND() {
  return getToken(CParser::ANDAND, 0);
}

CParser::AndExprContext::AndExprContext(CondExprContext *ctx) { copyFrom(ctx); }


std::any CParser::AndExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitAndExpr(this);
  else
    return visitor->visitChildren(this);
}

CParser::CondExprContext* CParser::condExpr() {
   return condExpr(0);
}

CParser::CondExprContext* CParser::condExpr(int precedence) {
  ParserRuleContext *parentContext = _ctx;
  size_t parentState = getState();
  CParser::CondExprContext *_localctx = _tracker.createInstance<CondExprContext>(_ctx, parentState);
  CParser::CondExprContext *previousContext = _localctx;
  (void)previousContext; // Silence compiler, in case the context is not used by generated code.
  size_t startState = 42;
  enterRecursionRule(_localctx, 42, CParser::RuleCondExpr, precedence);

    size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    unrollRecursionContexts(parentContext);
  });
  try {
    size_t alt;
    enterOuterAlt(_localctx, 1);
    setState(533);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 65, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<PostIncDecVarExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;

      setState(461);
      match(CParser::IDENT);
      setState(462);
      antlrcpp::downCast<PostIncDecVarExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == CParser::INC

      || _la == CParser::DEC)) {
        antlrcpp::downCast<PostIncDecVarExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PostIncDecIndexExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(463);
      match(CParser::IDENT);
      setState(464);
      match(CParser::LBRACK);
      setState(465);
      antlrcpp::downCast<PostIncDecIndexExprContext *>(_localctx)->idx = expr();
      setState(466);
      match(CParser::RBRACK);
      setState(467);
      antlrcpp::downCast<PostIncDecIndexExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == CParser::INC

      || _la == CParser::DEC)) {
        antlrcpp::downCast<PostIncDecIndexExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<PostIncDecDerefExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(469);
      match(CParser::STAR);
      setState(470);
      antlrcpp::downCast<PostIncDecDerefExprContext *>(_localctx)->ptr = condExpr(0);
      setState(471);
      antlrcpp::downCast<PostIncDecDerefExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == CParser::INC

      || _la == CParser::DEC)) {
        antlrcpp::downCast<PostIncDecDerefExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CallExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(473);
      match(CParser::IDENT);
      setState(474);
      match(CParser::LPAREN);
      setState(476);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 532592722176) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 963) != 0)) {
        setState(475);
        argList();
      }
      setState(478);
      match(CParser::RPAREN);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<CallPtrExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(479);
      match(CParser::LPAREN);
      setState(480);
      expr();
      setState(481);
      match(CParser::RPAREN);
      setState(482);
      match(CParser::LPAREN);
      setState(484);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 532592722176) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 963) != 0)) {
        setState(483);
        argList();
      }
      setState(486);
      match(CParser::RPAREN);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<IndexExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(488);
      match(CParser::IDENT);
      setState(489);
      match(CParser::LBRACK);
      setState(490);
      expr();
      setState(491);
      match(CParser::RBRACK);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<VarExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(493);
      match(CParser::IDENT);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<NumExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(494);
      match(CParser::NUMBER);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<CharExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(495);
      match(CParser::CHARLIT);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<StringLitExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(496);
      match(CParser::STRINGLIT);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<ParenExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(497);
      match(CParser::LPAREN);
      setState(498);
      expr();
      setState(499);
      match(CParser::RPAREN);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<CastExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(501);
      match(CParser::LPAREN);
      setState(502);
      typeSpec();
      setState(503);
      match(CParser::RPAREN);
      setState(504);
      condExpr(22);
      break;
    }

    case 13: {
      _localctx = _tracker.createInstance<SizeofTypeExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(506);
      match(CParser::SIZEOF_KW);
      setState(507);
      match(CParser::LPAREN);
      setState(508);
      typeSpec();
      setState(509);
      match(CParser::RPAREN);
      break;
    }

    case 14: {
      _localctx = _tracker.createInstance<SizeofExprExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(511);
      match(CParser::SIZEOF_KW);
      setState(512);
      match(CParser::LPAREN);
      setState(513);
      expr();
      setState(514);
      match(CParser::RPAREN);
      break;
    }

    case 15: {
      _localctx = _tracker.createInstance<DerefExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(516);
      match(CParser::STAR);
      setState(517);
      condExpr(19);
      break;
    }

    case 16: {
      _localctx = _tracker.createInstance<AddrOfExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(518);
      match(CParser::AMP);
      setState(519);
      condExpr(18);
      break;
    }

    case 17: {
      _localctx = _tracker.createInstance<UnaryExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(520);
      antlrcpp::downCast<UnaryExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 481036337152) != 0))) {
        antlrcpp::downCast<UnaryExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(521);
      condExpr(15);
      break;
    }

    case 18: {
      _localctx = _tracker.createInstance<PreIncDecVarExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(522);
      antlrcpp::downCast<PreIncDecVarExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == CParser::INC

      || _la == CParser::DEC)) {
        antlrcpp::downCast<PreIncDecVarExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(523);
      match(CParser::IDENT);
      break;
    }

    case 19: {
      _localctx = _tracker.createInstance<PreIncDecIndexExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(524);
      antlrcpp::downCast<PreIncDecIndexExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == CParser::INC

      || _la == CParser::DEC)) {
        antlrcpp::downCast<PreIncDecIndexExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(525);
      match(CParser::IDENT);
      setState(526);
      match(CParser::LBRACK);
      setState(527);
      antlrcpp::downCast<PreIncDecIndexExprContext *>(_localctx)->idx = expr();
      setState(528);
      match(CParser::RBRACK);
      break;
    }

    case 20: {
      _localctx = _tracker.createInstance<PreIncDecDerefExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(530);
      antlrcpp::downCast<PreIncDecDerefExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == CParser::INC

      || _la == CParser::DEC)) {
        antlrcpp::downCast<PreIncDecDerefExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(531);
      match(CParser::STAR);
      setState(532);
      antlrcpp::downCast<PreIncDecDerefExprContext *>(_localctx)->ptr = condExpr(12);
      break;
    }

    default:
      break;
    }
    _ctx->stop = _input->LT(-1);
    setState(591);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 69, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        setState(589);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 68, _ctx)) {
        case 1: {
          auto newContext = _tracker.createInstance<MulExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(535);

          if (!(precpred(_ctx, 11))) throw FailedPredicateException(this, "precpred(_ctx, 11)");
          setState(536);
          antlrcpp::downCast<MulExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!((((_la & ~ 0x3fULL) == 0) &&
            ((1ULL << _la) & 1666447310848) != 0))) {
            antlrcpp::downCast<MulExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(537);
          condExpr(12);
          break;
        }

        case 2: {
          auto newContext = _tracker.createInstance<AddExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(538);

          if (!(precpred(_ctx, 10))) throw FailedPredicateException(this, "precpred(_ctx, 10)");
          setState(539);
          antlrcpp::downCast<AddExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!(_la == CParser::MINUS

          || _la == CParser::PLUS)) {
            antlrcpp::downCast<AddExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(540);
          condExpr(11);
          break;
        }

        case 3: {
          auto newContext = _tracker.createInstance<ShiftExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(541);

          if (!(precpred(_ctx, 9))) throw FailedPredicateException(this, "precpred(_ctx, 9)");
          setState(542);
          antlrcpp::downCast<ShiftExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!(_la == CParser::SHL

          || _la == CParser::SHR)) {
            antlrcpp::downCast<ShiftExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(543);
          condExpr(10);
          break;
        }

        case 4: {
          auto newContext = _tracker.createInstance<RelExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(544);

          if (!(precpred(_ctx, 8))) throw FailedPredicateException(this, "precpred(_ctx, 8)");
          setState(545);
          antlrcpp::downCast<RelExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!((((_la & ~ 0x3fULL) == 0) &&
            ((1ULL << _la) & 263882790666240) != 0))) {
            antlrcpp::downCast<RelExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(546);
          condExpr(9);
          break;
        }

        case 5: {
          auto newContext = _tracker.createInstance<EqExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(547);

          if (!(precpred(_ctx, 7))) throw FailedPredicateException(this, "precpred(_ctx, 7)");
          setState(548);
          antlrcpp::downCast<EqExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!(_la == CParser::EQ

          || _la == CParser::NE)) {
            antlrcpp::downCast<EqExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(549);
          condExpr(8);
          break;
        }

        case 6: {
          auto newContext = _tracker.createInstance<BitAndExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(550);

          if (!(precpred(_ctx, 6))) throw FailedPredicateException(this, "precpred(_ctx, 6)");
          setState(551);
          match(CParser::AMP);
          setState(552);
          condExpr(7);
          break;
        }

        case 7: {
          auto newContext = _tracker.createInstance<BitXorExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(553);

          if (!(precpred(_ctx, 5))) throw FailedPredicateException(this, "precpred(_ctx, 5)");
          setState(554);
          match(CParser::CARET);
          setState(555);
          condExpr(6);
          break;
        }

        case 8: {
          auto newContext = _tracker.createInstance<BitOrExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(556);

          if (!(precpred(_ctx, 4))) throw FailedPredicateException(this, "precpred(_ctx, 4)");
          setState(557);
          match(CParser::PIPE);
          setState(558);
          condExpr(5);
          break;
        }

        case 9: {
          auto newContext = _tracker.createInstance<AndExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(559);

          if (!(precpred(_ctx, 3))) throw FailedPredicateException(this, "precpred(_ctx, 3)");
          setState(560);
          match(CParser::ANDAND);
          setState(561);
          condExpr(4);
          break;
        }

        case 10: {
          auto newContext = _tracker.createInstance<OrExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(562);

          if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
          setState(563);
          match(CParser::OROR);
          setState(564);
          condExpr(3);
          break;
        }

        case 11: {
          auto newContext = _tracker.createInstance<TernaryExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          newContext->cond = previousContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(565);

          if (!(precpred(_ctx, 1))) throw FailedPredicateException(this, "precpred(_ctx, 1)");
          setState(566);
          match(CParser::QUESTION);
          setState(567);
          antlrcpp::downCast<TernaryExprContext *>(_localctx)->then = expr();
          setState(568);
          match(CParser::COLON);
          setState(569);
          antlrcpp::downCast<TernaryExprContext *>(_localctx)->els = condExpr(1);
          break;
        }

        case 12: {
          auto newContext = _tracker.createInstance<MemberAccessExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          newContext->base = previousContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(571);

          if (!(precpred(_ctx, 17))) throw FailedPredicateException(this, "precpred(_ctx, 17)");
          setState(572);
          match(CParser::DOT);
          setState(573);
          antlrcpp::downCast<MemberAccessExprContext *>(_localctx)->name = match(CParser::IDENT);
          setState(578);
          _errHandler->sync(this);

          switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 66, _ctx)) {
          case 1: {
            setState(574);
            match(CParser::LBRACK);
            setState(575);
            antlrcpp::downCast<MemberAccessExprContext *>(_localctx)->idx = expr();
            setState(576);
            match(CParser::RBRACK);
            break;
          }

          default:
            break;
          }
          break;
        }

        case 13: {
          auto newContext = _tracker.createInstance<MemberAccessPtrExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          newContext->base = previousContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(580);

          if (!(precpred(_ctx, 16))) throw FailedPredicateException(this, "precpred(_ctx, 16)");
          setState(581);
          match(CParser::ARROW);
          setState(582);
          antlrcpp::downCast<MemberAccessPtrExprContext *>(_localctx)->name = match(CParser::IDENT);
          setState(587);
          _errHandler->sync(this);

          switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 67, _ctx)) {
          case 1: {
            setState(583);
            match(CParser::LBRACK);
            setState(584);
            antlrcpp::downCast<MemberAccessPtrExprContext *>(_localctx)->idx = expr();
            setState(585);
            match(CParser::RBRACK);
            break;
          }

          default:
            break;
          }
          break;
        }

        default:
          break;
        } 
      }
      setState(593);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 69, _ctx);
    }
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }
  return _localctx;
}

//----------------- ArgListContext ------------------------------------------------------------------

CParser::ArgListContext::ArgListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<CParser::ExprContext *> CParser::ArgListContext::expr() {
  return getRuleContexts<CParser::ExprContext>();
}

CParser::ExprContext* CParser::ArgListContext::expr(size_t i) {
  return getRuleContext<CParser::ExprContext>(i);
}

std::vector<tree::TerminalNode *> CParser::ArgListContext::COMMA() {
  return getTokens(CParser::COMMA);
}

tree::TerminalNode* CParser::ArgListContext::COMMA(size_t i) {
  return getToken(CParser::COMMA, i);
}


size_t CParser::ArgListContext::getRuleIndex() const {
  return CParser::RuleArgList;
}


std::any CParser::ArgListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitArgList(this);
  else
    return visitor->visitChildren(this);
}

CParser::ArgListContext* CParser::argList() {
  ArgListContext *_localctx = _tracker.createInstance<ArgListContext>(_ctx, getState());
  enterRule(_localctx, 44, CParser::RuleArgList);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(594);
    expr();
    setState(599);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(595);
      match(CParser::COMMA);
      setState(596);
      expr();
      setState(601);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

bool CParser::sempred(RuleContext *context, size_t ruleIndex, size_t predicateIndex) {
  switch (ruleIndex) {
    case 21: return condExprSempred(antlrcpp::downCast<CondExprContext *>(context), predicateIndex);

  default:
    break;
  }
  return true;
}

bool CParser::condExprSempred(CondExprContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 0: return precpred(_ctx, 11);
    case 1: return precpred(_ctx, 10);
    case 2: return precpred(_ctx, 9);
    case 3: return precpred(_ctx, 8);
    case 4: return precpred(_ctx, 7);
    case 5: return precpred(_ctx, 6);
    case 6: return precpred(_ctx, 5);
    case 7: return precpred(_ctx, 4);
    case 8: return precpred(_ctx, 3);
    case 9: return precpred(_ctx, 2);
    case 10: return precpred(_ctx, 1);
    case 11: return precpred(_ctx, 17);
    case 12: return precpred(_ctx, 16);

  default:
    break;
  }
  return true;
}

void CParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  cparserParserInitialize();
#else
  ::antlr4::internal::call_once(cparserParserOnceFlag, cparserParserInitialize);
#endif
}
