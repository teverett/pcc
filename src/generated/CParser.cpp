
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
      "program", "typeSpec", "baseTypeSpec", "topLevelDecl", "functionDecl", 
      "paramList", "param", "globalDecl", "arrayInit", "arrayInitElem", 
      "block", "statement", "simpleStmt", "assignOp", "expr", "condExpr", 
      "argList"
    },
    std::vector<std::string>{
      "", "'int'", "'char'", "'short'", "'long'", "'signed'", "'unsigned'", 
      "'sizeof'", "'if'", "'else'", "'while'", "'for'", "'return'", "'break'", 
      "'continue'", "'extern'", "'('", "')'", "'['", "']'", "'{'", "", "'@'", 
      "';'", "','", "'='", "'*'", "'&'", "'-'", "'!'", "'~'", "'/'", "'%'", 
      "'+'", "'<<'", "'>>'", "'<='", "'>='", "'<'", "'>'", "'=='", "'!='", 
      "'^'", "'|'", "'&&'", "'||'", "'+='", "'-='", "'*='", "'/='", "'%='", 
      "'&='", "'^='", "'|='", "'<<='", "'>>='", "'++'", "'--'", "'\\u003F'", 
      "':'"
    },
    std::vector<std::string>{
      "", "INT_KW", "CHAR_KW", "SHORT_KW", "LONG_KW", "SIGNED_KW", "UNSIGNED_KW", 
      "SIZEOF_KW", "IF_KW", "ELSE_KW", "WHILE_KW", "FOR_KW", "RETURN_KW", 
      "BREAK_KW", "CONTINUE_KW", "EXTERN_KW", "LPAREN", "RPAREN", "LBRACK", 
      "RBRACK", "LBRACE", "RBRACE", "AT", "SEMI", "COMMA", "ASSIGN", "STAR", 
      "AMP", "MINUS", "BANG", "TILDE", "SLASH", "PERCENT", "PLUS", "SHL", 
      "SHR", "LE", "GE", "LT", "GT", "EQ", "NE", "CARET", "PIPE", "ANDAND", 
      "OROR", "PLUSEQ", "MINUSEQ", "STAREQ", "SLASHEQ", "PERCENTEQ", "AMPEQ", 
      "CARETEQ", "PIPEEQ", "SHLEQ", "SHREQ", "INC", "DEC", "QUESTION", "COLON", 
      "IDENT", "NUMBER", "CHARLIT", "STRINGLIT", "LINE_COMMENT", "BLOCK_COMMENT", 
      "WS", "ASM_BEGIN", "ASM_TEXT", "ASM_END"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,69,411,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,2,16,7,16,1,0,5,0,36,8,0,10,0,12,0,39,9,0,1,0,1,0,1,1,1,
  	1,5,1,45,8,1,10,1,12,1,48,9,1,1,2,3,2,51,8,2,1,2,1,2,3,2,55,8,2,1,2,1,
  	2,3,2,59,8,2,1,2,3,2,62,8,2,1,2,1,2,3,2,66,8,2,1,2,1,2,1,2,3,2,71,8,2,
  	1,2,3,2,74,8,2,1,2,1,2,3,2,78,8,2,1,2,3,2,81,8,2,1,3,1,3,3,3,85,8,3,1,
  	4,1,4,1,4,1,4,1,4,3,4,92,8,4,1,4,1,4,1,4,1,4,1,4,1,4,1,4,3,4,101,8,4,
  	1,4,1,4,1,4,3,4,106,8,4,1,4,1,4,3,4,110,8,4,3,4,112,8,4,1,5,1,5,1,5,5,
  	5,117,8,5,10,5,12,5,120,9,5,1,6,1,6,1,6,1,7,1,7,1,7,1,7,1,7,1,7,1,7,1,
  	7,1,7,1,7,1,7,1,7,1,7,1,7,1,7,1,7,1,7,1,7,1,7,1,7,3,7,145,8,7,1,7,1,7,
  	3,7,149,8,7,1,7,1,7,1,7,1,7,1,7,1,7,3,7,157,8,7,1,7,3,7,160,8,7,1,7,1,
  	7,3,7,164,8,7,1,7,1,7,3,7,168,8,7,1,8,1,8,1,8,1,8,5,8,174,8,8,10,8,12,
  	8,177,9,8,1,8,1,8,1,9,3,9,182,8,9,1,9,1,9,3,9,186,8,9,1,10,1,10,5,10,
  	190,8,10,10,10,12,10,193,9,10,1,10,1,10,1,11,1,11,1,11,1,11,1,11,1,11,
  	1,11,1,11,3,11,205,8,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,
  	3,11,216,8,11,1,11,1,11,3,11,220,8,11,1,11,1,11,3,11,224,8,11,1,11,1,
  	11,1,11,1,11,3,11,230,8,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,3,11,239,
  	8,11,1,11,1,11,1,11,1,11,1,11,3,11,246,8,11,1,12,1,12,1,12,1,12,1,12,
  	1,12,1,12,3,12,255,8,12,1,12,1,12,1,12,1,12,3,12,261,8,12,1,12,3,12,264,
  	8,12,1,13,1,13,1,14,1,14,1,14,1,14,1,14,1,14,1,14,1,14,1,14,1,14,1,14,
  	1,14,1,14,1,14,1,14,1,14,1,14,3,14,285,8,14,1,15,1,15,1,15,1,15,1,15,
  	1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,3,15,303,8,15,
  	1,15,1,15,1,15,1,15,1,15,1,15,3,15,311,8,15,1,15,1,15,1,15,1,15,1,15,
  	1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,
  	1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,
  	1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,
  	3,15,360,8,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,
  	1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,
  	1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,5,15,398,8,15,
  	10,15,12,15,401,9,15,1,16,1,16,1,16,5,16,406,8,16,10,16,12,16,409,9,16,
  	1,16,0,1,30,17,0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,0,9,1,0,
  	5,6,2,0,25,25,46,55,1,0,56,57,1,0,28,30,2,0,26,26,31,32,2,0,28,28,33,
  	33,1,0,34,35,1,0,36,39,1,0,40,41,482,0,37,1,0,0,0,2,42,1,0,0,0,4,80,1,
  	0,0,0,6,84,1,0,0,0,8,111,1,0,0,0,10,113,1,0,0,0,12,121,1,0,0,0,14,167,
  	1,0,0,0,16,169,1,0,0,0,18,185,1,0,0,0,20,187,1,0,0,0,22,245,1,0,0,0,24,
  	263,1,0,0,0,26,265,1,0,0,0,28,284,1,0,0,0,30,359,1,0,0,0,32,402,1,0,0,
  	0,34,36,3,6,3,0,35,34,1,0,0,0,36,39,1,0,0,0,37,35,1,0,0,0,37,38,1,0,0,
  	0,38,40,1,0,0,0,39,37,1,0,0,0,40,41,5,0,0,1,41,1,1,0,0,0,42,46,3,4,2,
  	0,43,45,5,26,0,0,44,43,1,0,0,0,45,48,1,0,0,0,46,44,1,0,0,0,46,47,1,0,
  	0,0,47,3,1,0,0,0,48,46,1,0,0,0,49,51,7,0,0,0,50,49,1,0,0,0,50,51,1,0,
  	0,0,51,52,1,0,0,0,52,81,5,2,0,0,53,55,7,0,0,0,54,53,1,0,0,0,54,55,1,0,
  	0,0,55,56,1,0,0,0,56,58,5,3,0,0,57,59,5,1,0,0,58,57,1,0,0,0,58,59,1,0,
  	0,0,59,81,1,0,0,0,60,62,7,0,0,0,61,60,1,0,0,0,61,62,1,0,0,0,62,63,1,0,
  	0,0,63,81,5,1,0,0,64,66,7,0,0,0,65,64,1,0,0,0,65,66,1,0,0,0,66,67,1,0,
  	0,0,67,68,5,4,0,0,68,70,5,4,0,0,69,71,5,1,0,0,70,69,1,0,0,0,70,71,1,0,
  	0,0,71,81,1,0,0,0,72,74,7,0,0,0,73,72,1,0,0,0,73,74,1,0,0,0,74,75,1,0,
  	0,0,75,77,5,4,0,0,76,78,5,1,0,0,77,76,1,0,0,0,77,78,1,0,0,0,78,81,1,0,
  	0,0,79,81,7,0,0,0,80,50,1,0,0,0,80,54,1,0,0,0,80,61,1,0,0,0,80,65,1,0,
  	0,0,80,73,1,0,0,0,80,79,1,0,0,0,81,5,1,0,0,0,82,85,3,8,4,0,83,85,3,14,
  	7,0,84,82,1,0,0,0,84,83,1,0,0,0,85,7,1,0,0,0,86,87,5,15,0,0,87,88,3,2,
  	1,0,88,89,5,60,0,0,89,91,5,16,0,0,90,92,3,10,5,0,91,90,1,0,0,0,91,92,
  	1,0,0,0,92,93,1,0,0,0,93,94,5,17,0,0,94,95,5,23,0,0,95,112,1,0,0,0,96,
  	97,3,2,1,0,97,98,5,60,0,0,98,100,5,16,0,0,99,101,3,10,5,0,100,99,1,0,
  	0,0,100,101,1,0,0,0,101,102,1,0,0,0,102,105,5,17,0,0,103,104,5,22,0,0,
  	104,106,5,61,0,0,105,103,1,0,0,0,105,106,1,0,0,0,106,109,1,0,0,0,107,
  	110,3,20,10,0,108,110,5,23,0,0,109,107,1,0,0,0,109,108,1,0,0,0,110,112,
  	1,0,0,0,111,86,1,0,0,0,111,96,1,0,0,0,112,9,1,0,0,0,113,118,3,12,6,0,
  	114,115,5,24,0,0,115,117,3,12,6,0,116,114,1,0,0,0,117,120,1,0,0,0,118,
  	116,1,0,0,0,118,119,1,0,0,0,119,11,1,0,0,0,120,118,1,0,0,0,121,122,3,
  	2,1,0,122,123,5,60,0,0,123,13,1,0,0,0,124,125,5,15,0,0,125,126,3,2,1,
  	0,126,127,5,60,0,0,127,128,5,18,0,0,128,129,5,61,0,0,129,130,5,19,0,0,
  	130,131,5,23,0,0,131,168,1,0,0,0,132,133,5,15,0,0,133,134,3,2,1,0,134,
  	135,5,60,0,0,135,136,5,23,0,0,136,168,1,0,0,0,137,138,3,2,1,0,138,139,
  	5,60,0,0,139,140,5,18,0,0,140,141,5,61,0,0,141,144,5,19,0,0,142,143,5,
  	25,0,0,143,145,3,16,8,0,144,142,1,0,0,0,144,145,1,0,0,0,145,148,1,0,0,
  	0,146,147,5,22,0,0,147,149,5,61,0,0,148,146,1,0,0,0,148,149,1,0,0,0,149,
  	150,1,0,0,0,150,151,5,23,0,0,151,168,1,0,0,0,152,153,3,2,1,0,153,159,
  	5,60,0,0,154,156,5,25,0,0,155,157,5,28,0,0,156,155,1,0,0,0,156,157,1,
  	0,0,0,157,158,1,0,0,0,158,160,5,61,0,0,159,154,1,0,0,0,159,160,1,0,0,
  	0,160,163,1,0,0,0,161,162,5,22,0,0,162,164,5,61,0,0,163,161,1,0,0,0,163,
  	164,1,0,0,0,164,165,1,0,0,0,165,166,5,23,0,0,166,168,1,0,0,0,167,124,
  	1,0,0,0,167,132,1,0,0,0,167,137,1,0,0,0,167,152,1,0,0,0,168,15,1,0,0,
  	0,169,170,5,20,0,0,170,175,3,18,9,0,171,172,5,24,0,0,172,174,3,18,9,0,
  	173,171,1,0,0,0,174,177,1,0,0,0,175,173,1,0,0,0,175,176,1,0,0,0,176,178,
  	1,0,0,0,177,175,1,0,0,0,178,179,5,21,0,0,179,17,1,0,0,0,180,182,5,28,
  	0,0,181,180,1,0,0,0,181,182,1,0,0,0,182,183,1,0,0,0,183,186,5,61,0,0,
  	184,186,5,60,0,0,185,181,1,0,0,0,185,184,1,0,0,0,186,19,1,0,0,0,187,191,
  	5,20,0,0,188,190,3,22,11,0,189,188,1,0,0,0,190,193,1,0,0,0,191,189,1,
  	0,0,0,191,192,1,0,0,0,192,194,1,0,0,0,193,191,1,0,0,0,194,195,5,21,0,
  	0,195,21,1,0,0,0,196,246,3,20,10,0,197,198,5,8,0,0,198,199,5,16,0,0,199,
  	200,3,28,14,0,200,201,5,17,0,0,201,204,3,22,11,0,202,203,5,9,0,0,203,
  	205,3,22,11,0,204,202,1,0,0,0,204,205,1,0,0,0,205,246,1,0,0,0,206,207,
  	5,10,0,0,207,208,5,16,0,0,208,209,3,28,14,0,209,210,5,17,0,0,210,211,
  	3,22,11,0,211,246,1,0,0,0,212,213,5,11,0,0,213,215,5,16,0,0,214,216,3,
  	24,12,0,215,214,1,0,0,0,215,216,1,0,0,0,216,217,1,0,0,0,217,219,5,23,
  	0,0,218,220,3,28,14,0,219,218,1,0,0,0,219,220,1,0,0,0,220,221,1,0,0,0,
  	221,223,5,23,0,0,222,224,3,24,12,0,223,222,1,0,0,0,223,224,1,0,0,0,224,
  	225,1,0,0,0,225,226,5,17,0,0,226,246,3,22,11,0,227,229,5,12,0,0,228,230,
  	3,28,14,0,229,228,1,0,0,0,229,230,1,0,0,0,230,231,1,0,0,0,231,246,5,23,
  	0,0,232,233,5,13,0,0,233,246,5,23,0,0,234,235,5,14,0,0,235,246,5,23,0,
  	0,236,238,5,67,0,0,237,239,5,68,0,0,238,237,1,0,0,0,238,239,1,0,0,0,239,
  	240,1,0,0,0,240,246,5,69,0,0,241,242,3,24,12,0,242,243,5,23,0,0,243,246,
  	1,0,0,0,244,246,5,23,0,0,245,196,1,0,0,0,245,197,1,0,0,0,245,206,1,0,
  	0,0,245,212,1,0,0,0,245,227,1,0,0,0,245,232,1,0,0,0,245,234,1,0,0,0,245,
  	236,1,0,0,0,245,241,1,0,0,0,245,244,1,0,0,0,246,23,1,0,0,0,247,248,3,
  	2,1,0,248,249,5,60,0,0,249,250,5,18,0,0,250,251,5,61,0,0,251,254,5,19,
  	0,0,252,253,5,25,0,0,253,255,3,16,8,0,254,252,1,0,0,0,254,255,1,0,0,0,
  	255,264,1,0,0,0,256,257,3,2,1,0,257,260,5,60,0,0,258,259,5,25,0,0,259,
  	261,3,28,14,0,260,258,1,0,0,0,260,261,1,0,0,0,261,264,1,0,0,0,262,264,
  	3,28,14,0,263,247,1,0,0,0,263,256,1,0,0,0,263,262,1,0,0,0,264,25,1,0,
  	0,0,265,266,7,1,0,0,266,27,1,0,0,0,267,268,5,26,0,0,268,269,3,30,15,0,
  	269,270,3,26,13,0,270,271,3,28,14,0,271,285,1,0,0,0,272,273,5,60,0,0,
  	273,274,5,18,0,0,274,275,3,28,14,0,275,276,5,19,0,0,276,277,3,26,13,0,
  	277,278,3,28,14,0,278,285,1,0,0,0,279,280,5,60,0,0,280,281,3,26,13,0,
  	281,282,3,28,14,0,282,285,1,0,0,0,283,285,3,30,15,0,284,267,1,0,0,0,284,
  	272,1,0,0,0,284,279,1,0,0,0,284,283,1,0,0,0,285,29,1,0,0,0,286,287,6,
  	15,-1,0,287,288,5,60,0,0,288,360,7,2,0,0,289,290,5,60,0,0,290,291,5,18,
  	0,0,291,292,3,28,14,0,292,293,5,19,0,0,293,294,7,2,0,0,294,360,1,0,0,
  	0,295,296,5,26,0,0,296,297,3,30,15,0,297,298,7,2,0,0,298,360,1,0,0,0,
  	299,300,5,60,0,0,300,302,5,16,0,0,301,303,3,32,16,0,302,301,1,0,0,0,302,
  	303,1,0,0,0,303,304,1,0,0,0,304,360,5,17,0,0,305,306,5,16,0,0,306,307,
  	3,28,14,0,307,308,5,17,0,0,308,310,5,16,0,0,309,311,3,32,16,0,310,309,
  	1,0,0,0,310,311,1,0,0,0,311,312,1,0,0,0,312,313,5,17,0,0,313,360,1,0,
  	0,0,314,315,5,60,0,0,315,316,5,18,0,0,316,317,3,28,14,0,317,318,5,19,
  	0,0,318,360,1,0,0,0,319,360,5,60,0,0,320,360,5,61,0,0,321,360,5,62,0,
  	0,322,360,5,63,0,0,323,324,5,16,0,0,324,325,3,28,14,0,325,326,5,17,0,
  	0,326,360,1,0,0,0,327,328,5,16,0,0,328,329,3,2,1,0,329,330,5,17,0,0,330,
  	331,3,30,15,20,331,360,1,0,0,0,332,333,5,7,0,0,333,334,5,16,0,0,334,335,
  	3,2,1,0,335,336,5,17,0,0,336,360,1,0,0,0,337,338,5,7,0,0,338,339,5,16,
  	0,0,339,340,3,28,14,0,340,341,5,17,0,0,341,360,1,0,0,0,342,343,5,26,0,
  	0,343,360,3,30,15,17,344,345,5,27,0,0,345,360,3,30,15,16,346,347,7,3,
  	0,0,347,360,3,30,15,15,348,349,7,2,0,0,349,360,5,60,0,0,350,351,7,2,0,
  	0,351,352,5,60,0,0,352,353,5,18,0,0,353,354,3,28,14,0,354,355,5,19,0,
  	0,355,360,1,0,0,0,356,357,7,2,0,0,357,358,5,26,0,0,358,360,3,30,15,12,
  	359,286,1,0,0,0,359,289,1,0,0,0,359,295,1,0,0,0,359,299,1,0,0,0,359,305,
  	1,0,0,0,359,314,1,0,0,0,359,319,1,0,0,0,359,320,1,0,0,0,359,321,1,0,0,
  	0,359,322,1,0,0,0,359,323,1,0,0,0,359,327,1,0,0,0,359,332,1,0,0,0,359,
  	337,1,0,0,0,359,342,1,0,0,0,359,344,1,0,0,0,359,346,1,0,0,0,359,348,1,
  	0,0,0,359,350,1,0,0,0,359,356,1,0,0,0,360,399,1,0,0,0,361,362,10,11,0,
  	0,362,363,7,4,0,0,363,398,3,30,15,12,364,365,10,10,0,0,365,366,7,5,0,
  	0,366,398,3,30,15,11,367,368,10,9,0,0,368,369,7,6,0,0,369,398,3,30,15,
  	10,370,371,10,8,0,0,371,372,7,7,0,0,372,398,3,30,15,9,373,374,10,7,0,
  	0,374,375,7,8,0,0,375,398,3,30,15,8,376,377,10,6,0,0,377,378,5,27,0,0,
  	378,398,3,30,15,7,379,380,10,5,0,0,380,381,5,42,0,0,381,398,3,30,15,6,
  	382,383,10,4,0,0,383,384,5,43,0,0,384,398,3,30,15,5,385,386,10,3,0,0,
  	386,387,5,44,0,0,387,398,3,30,15,4,388,389,10,2,0,0,389,390,5,45,0,0,
  	390,398,3,30,15,3,391,392,10,1,0,0,392,393,5,58,0,0,393,394,3,28,14,0,
  	394,395,5,59,0,0,395,396,3,30,15,1,396,398,1,0,0,0,397,361,1,0,0,0,397,
  	364,1,0,0,0,397,367,1,0,0,0,397,370,1,0,0,0,397,373,1,0,0,0,397,376,1,
  	0,0,0,397,379,1,0,0,0,397,382,1,0,0,0,397,385,1,0,0,0,397,388,1,0,0,0,
  	397,391,1,0,0,0,398,401,1,0,0,0,399,397,1,0,0,0,399,400,1,0,0,0,400,31,
  	1,0,0,0,401,399,1,0,0,0,402,407,3,28,14,0,403,404,5,24,0,0,404,406,3,
  	28,14,0,405,403,1,0,0,0,406,409,1,0,0,0,407,405,1,0,0,0,407,408,1,0,0,
  	0,408,33,1,0,0,0,409,407,1,0,0,0,45,37,46,50,54,58,61,65,70,73,77,80,
  	84,91,100,105,109,111,118,144,148,156,159,163,167,175,181,185,191,204,
  	215,219,223,229,238,245,254,260,263,284,302,310,359,397,399,407
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
    setState(37);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 32894) != 0)) {
      setState(34);
      topLevelDecl();
      setState(39);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(40);
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
    setState(42);
    baseTypeSpec();
    setState(46);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::STAR) {
      setState(43);
      match(CParser::STAR);
      setState(48);
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
    setState(80);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 10, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::CharTypeContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(50);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(49);
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
      setState(52);
      match(CParser::CHAR_KW);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::ShortTypeContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(54);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(53);
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
      setState(56);
      match(CParser::SHORT_KW);
      setState(58);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::INT_KW) {
        setState(57);
        match(CParser::INT_KW);
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::IntTypeContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(61);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(60);
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
      setState(63);
      match(CParser::INT_KW);
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::LongLongTypeContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(65);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::SIGNED_KW

      || _la == CParser::UNSIGNED_KW) {
        setState(64);
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
      setState(67);
      match(CParser::LONG_KW);
      setState(68);
      match(CParser::LONG_KW);
      setState(70);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::INT_KW) {
        setState(69);
        match(CParser::INT_KW);
      }
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<CParser::LongTypeContext>(_localctx);
      enterOuterAlt(_localctx, 5);
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
      match(CParser::LONG_KW);
      setState(77);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::INT_KW) {
        setState(76);
        match(CParser::INT_KW);
      }
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<CParser::SignedOrUnsignedTypeContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(79);
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
    setState(84);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 11, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(82);
      functionDecl();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(83);
      globalDecl();
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

CParser::TypeSpecContext* CParser::ExternFunctionDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
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

CParser::TypeSpecContext* CParser::LocalFunctionDeclContext::typeSpec() {
  return getRuleContext<CParser::TypeSpecContext>(0);
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
  enterRule(_localctx, 8, CParser::RuleFunctionDecl);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(111);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case CParser::EXTERN_KW: {
        _localctx = _tracker.createInstance<CParser::ExternFunctionDeclContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(86);
        match(CParser::EXTERN_KW);
        setState(87);
        typeSpec();
        setState(88);
        match(CParser::IDENT);
        setState(89);
        match(CParser::LPAREN);
        setState(91);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 126) != 0)) {
          setState(90);
          paramList();
        }
        setState(93);
        match(CParser::RPAREN);
        setState(94);
        match(CParser::SEMI);
        break;
      }

      case CParser::INT_KW:
      case CParser::CHAR_KW:
      case CParser::SHORT_KW:
      case CParser::LONG_KW:
      case CParser::SIGNED_KW:
      case CParser::UNSIGNED_KW: {
        _localctx = _tracker.createInstance<CParser::LocalFunctionDeclContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(96);
        typeSpec();
        setState(97);
        match(CParser::IDENT);
        setState(98);
        match(CParser::LPAREN);
        setState(100);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 126) != 0)) {
          setState(99);
          paramList();
        }
        setState(102);
        match(CParser::RPAREN);
        setState(105);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::AT) {
          setState(103);
          match(CParser::AT);
          setState(104);
          antlrcpp::downCast<LocalFunctionDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
        }
        setState(109);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case CParser::LBRACE: {
            setState(107);
            block();
            break;
          }

          case CParser::SEMI: {
            setState(108);
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
  enterRule(_localctx, 10, CParser::RuleParamList);
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
    setState(113);
    param();
    setState(118);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(114);
      match(CParser::COMMA);
      setState(115);
      param();
      setState(120);
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
  enterRule(_localctx, 12, CParser::RuleParam);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(121);
    typeSpec();
    setState(122);
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

tree::TerminalNode* CParser::GlobalScalarDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
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
  enterRule(_localctx, 14, CParser::RuleGlobalDecl);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(167);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 23, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::ExternArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(124);
      match(CParser::EXTERN_KW);
      setState(125);
      typeSpec();
      setState(126);
      match(CParser::IDENT);
      setState(127);
      match(CParser::LBRACK);
      setState(128);
      antlrcpp::downCast<ExternArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(129);
      match(CParser::RBRACK);
      setState(130);
      match(CParser::SEMI);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::ExternScalarDeclContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(132);
      match(CParser::EXTERN_KW);
      setState(133);
      typeSpec();
      setState(134);
      match(CParser::IDENT);
      setState(135);
      match(CParser::SEMI);
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::GlobalArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(137);
      typeSpec();
      setState(138);
      match(CParser::IDENT);
      setState(139);
      match(CParser::LBRACK);
      setState(140);
      antlrcpp::downCast<GlobalArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(141);
      match(CParser::RBRACK);
      setState(144);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(142);
        match(CParser::ASSIGN);
        setState(143);
        arrayInit();
      }
      setState(148);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::AT) {
        setState(146);
        match(CParser::AT);
        setState(147);
        antlrcpp::downCast<GlobalArrayDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
      }
      setState(150);
      match(CParser::SEMI);
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::GlobalScalarDeclContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(152);
      typeSpec();
      setState(153);
      match(CParser::IDENT);
      setState(159);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(154);
        match(CParser::ASSIGN);
        setState(156);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::MINUS) {
          setState(155);
          antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->sign = match(CParser::MINUS);
        }
        setState(158);
        antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->val = match(CParser::NUMBER);
      }
      setState(163);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::AT) {
        setState(161);
        match(CParser::AT);
        setState(162);
        antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
      }
      setState(165);
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
  enterRule(_localctx, 16, CParser::RuleArrayInit);
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
    setState(169);
    match(CParser::LBRACE);
    setState(170);
    antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext = arrayInitElem();
    antlrcpp::downCast<ArrayInitContext *>(_localctx)->elem.push_back(antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext);
    setState(175);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(171);
      match(CParser::COMMA);
      setState(172);
      antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext = arrayInitElem();
      antlrcpp::downCast<ArrayInitContext *>(_localctx)->elem.push_back(antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext);
      setState(177);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(178);
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
  enterRule(_localctx, 18, CParser::RuleArrayInitElem);
  size_t _la = 0;

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
      case CParser::MINUS:
      case CParser::NUMBER: {
        _localctx = _tracker.createInstance<CParser::NumArrayElemContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(181);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::MINUS) {
          setState(180);
          antlrcpp::downCast<NumArrayElemContext *>(_localctx)->sign = match(CParser::MINUS);
        }
        setState(183);
        match(CParser::NUMBER);
        break;
      }

      case CParser::IDENT: {
        _localctx = _tracker.createInstance<CParser::SymArrayElemContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(184);
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
  enterRule(_localctx, 20, CParser::RuleBlock);
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
    match(CParser::LBRACE);
    setState(191);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -936748720403153410) != 0) || _la == CParser::ASM_BEGIN) {
      setState(188);
      statement();
      setState(193);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(194);
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
  enterRule(_localctx, 22, CParser::RuleStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(245);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case CParser::LBRACE: {
        _localctx = _tracker.createInstance<CParser::BlockStmtContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(196);
        block();
        break;
      }

      case CParser::IF_KW: {
        _localctx = _tracker.createInstance<CParser::IfStmtContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(197);
        match(CParser::IF_KW);
        setState(198);
        match(CParser::LPAREN);
        setState(199);
        expr();
        setState(200);
        match(CParser::RPAREN);
        setState(201);
        antlrcpp::downCast<IfStmtContext *>(_localctx)->thenS = statement();
        setState(204);
        _errHandler->sync(this);

        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 28, _ctx)) {
        case 1: {
          setState(202);
          match(CParser::ELSE_KW);
          setState(203);
          antlrcpp::downCast<IfStmtContext *>(_localctx)->elseS = statement();
          break;
        }

        default:
          break;
        }
        break;
      }

      case CParser::WHILE_KW: {
        _localctx = _tracker.createInstance<CParser::WhileStmtContext>(_localctx);
        enterOuterAlt(_localctx, 3);
        setState(206);
        match(CParser::WHILE_KW);
        setState(207);
        match(CParser::LPAREN);
        setState(208);
        expr();
        setState(209);
        match(CParser::RPAREN);
        setState(210);
        statement();
        break;
      }

      case CParser::FOR_KW: {
        _localctx = _tracker.createInstance<CParser::ForStmtContext>(_localctx);
        enterOuterAlt(_localctx, 4);
        setState(212);
        match(CParser::FOR_KW);
        setState(213);
        match(CParser::LPAREN);
        setState(215);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -936748720412622594) != 0)) {
          setState(214);
          antlrcpp::downCast<ForStmtContext *>(_localctx)->init = simpleStmt();
        }
        setState(217);
        match(CParser::SEMI);
        setState(219);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -936748720412622720) != 0)) {
          setState(218);
          antlrcpp::downCast<ForStmtContext *>(_localctx)->cond = expr();
        }
        setState(221);
        match(CParser::SEMI);
        setState(223);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -936748720412622594) != 0)) {
          setState(222);
          antlrcpp::downCast<ForStmtContext *>(_localctx)->post = simpleStmt();
        }
        setState(225);
        match(CParser::RPAREN);
        setState(226);
        statement();
        break;
      }

      case CParser::RETURN_KW: {
        _localctx = _tracker.createInstance<CParser::ReturnStmtContext>(_localctx);
        enterOuterAlt(_localctx, 5);
        setState(227);
        match(CParser::RETURN_KW);
        setState(229);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -936748720412622720) != 0)) {
          setState(228);
          expr();
        }
        setState(231);
        match(CParser::SEMI);
        break;
      }

      case CParser::BREAK_KW: {
        _localctx = _tracker.createInstance<CParser::BreakStmtContext>(_localctx);
        enterOuterAlt(_localctx, 6);
        setState(232);
        match(CParser::BREAK_KW);
        setState(233);
        match(CParser::SEMI);
        break;
      }

      case CParser::CONTINUE_KW: {
        _localctx = _tracker.createInstance<CParser::ContinueStmtContext>(_localctx);
        enterOuterAlt(_localctx, 7);
        setState(234);
        match(CParser::CONTINUE_KW);
        setState(235);
        match(CParser::SEMI);
        break;
      }

      case CParser::ASM_BEGIN: {
        _localctx = _tracker.createInstance<CParser::AsmStmtContext>(_localctx);
        enterOuterAlt(_localctx, 8);
        setState(236);
        match(CParser::ASM_BEGIN);
        setState(238);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::ASM_TEXT) {
          setState(237);
          antlrcpp::downCast<AsmStmtContext *>(_localctx)->text = match(CParser::ASM_TEXT);
        }
        setState(240);
        match(CParser::ASM_END);
        break;
      }

      case CParser::INT_KW:
      case CParser::CHAR_KW:
      case CParser::SHORT_KW:
      case CParser::LONG_KW:
      case CParser::SIGNED_KW:
      case CParser::UNSIGNED_KW:
      case CParser::SIZEOF_KW:
      case CParser::LPAREN:
      case CParser::STAR:
      case CParser::AMP:
      case CParser::MINUS:
      case CParser::BANG:
      case CParser::TILDE:
      case CParser::INC:
      case CParser::DEC:
      case CParser::IDENT:
      case CParser::NUMBER:
      case CParser::CHARLIT:
      case CParser::STRINGLIT: {
        _localctx = _tracker.createInstance<CParser::SimpleStmtWrapperContext>(_localctx);
        enterOuterAlt(_localctx, 9);
        setState(241);
        simpleStmt();
        setState(242);
        match(CParser::SEMI);
        break;
      }

      case CParser::SEMI: {
        _localctx = _tracker.createInstance<CParser::EmptyStmtContext>(_localctx);
        enterOuterAlt(_localctx, 10);
        setState(244);
        match(CParser::SEMI);
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
  enterRule(_localctx, 24, CParser::RuleSimpleStmt);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(263);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 37, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::LocalArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(247);
      typeSpec();
      setState(248);
      match(CParser::IDENT);
      setState(249);
      match(CParser::LBRACK);
      setState(250);
      antlrcpp::downCast<LocalArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(251);
      match(CParser::RBRACK);
      setState(254);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(252);
        match(CParser::ASSIGN);
        setState(253);
        arrayInit();
      }
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::LocalDeclContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(256);
      typeSpec();
      setState(257);
      match(CParser::IDENT);
      setState(260);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(258);
        match(CParser::ASSIGN);
        setState(259);
        expr();
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::ExprStmtContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(262);
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
  enterRule(_localctx, 26, CParser::RuleAssignOp);
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
    setState(265);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 71987225327304704) != 0))) {
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
  enterRule(_localctx, 28, CParser::RuleExpr);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(284);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 38, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::AssignDerefExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(267);
      match(CParser::STAR);
      setState(268);
      antlrcpp::downCast<AssignDerefExprContext *>(_localctx)->ptr = condExpr(0);
      setState(269);
      antlrcpp::downCast<AssignDerefExprContext *>(_localctx)->op = assignOp();
      setState(270);
      antlrcpp::downCast<AssignDerefExprContext *>(_localctx)->val = expr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::AssignIndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(272);
      match(CParser::IDENT);
      setState(273);
      match(CParser::LBRACK);
      setState(274);
      antlrcpp::downCast<AssignIndexExprContext *>(_localctx)->idx = expr();
      setState(275);
      match(CParser::RBRACK);
      setState(276);
      antlrcpp::downCast<AssignIndexExprContext *>(_localctx)->op = assignOp();
      setState(277);
      antlrcpp::downCast<AssignIndexExprContext *>(_localctx)->val = expr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(279);
      match(CParser::IDENT);
      setState(280);
      antlrcpp::downCast<AssignExprContext *>(_localctx)->op = assignOp();
      setState(281);
      antlrcpp::downCast<AssignExprContext *>(_localctx)->val = expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::ExprCondContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(283);
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
  size_t startState = 30;
  enterRecursionRule(_localctx, 30, CParser::RuleCondExpr, precedence);

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
    setState(359);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 41, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<PostIncDecVarExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;

      setState(287);
      match(CParser::IDENT);
      setState(288);
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
      setState(289);
      match(CParser::IDENT);
      setState(290);
      match(CParser::LBRACK);
      setState(291);
      antlrcpp::downCast<PostIncDecIndexExprContext *>(_localctx)->idx = expr();
      setState(292);
      match(CParser::RBRACK);
      setState(293);
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
      setState(295);
      match(CParser::STAR);
      setState(296);
      antlrcpp::downCast<PostIncDecDerefExprContext *>(_localctx)->ptr = condExpr(0);
      setState(297);
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
      setState(299);
      match(CParser::IDENT);
      setState(300);
      match(CParser::LPAREN);
      setState(302);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & -936748720412622720) != 0)) {
        setState(301);
        argList();
      }
      setState(304);
      match(CParser::RPAREN);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<CallPtrExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(305);
      match(CParser::LPAREN);
      setState(306);
      expr();
      setState(307);
      match(CParser::RPAREN);
      setState(308);
      match(CParser::LPAREN);
      setState(310);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & -936748720412622720) != 0)) {
        setState(309);
        argList();
      }
      setState(312);
      match(CParser::RPAREN);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<IndexExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(314);
      match(CParser::IDENT);
      setState(315);
      match(CParser::LBRACK);
      setState(316);
      expr();
      setState(317);
      match(CParser::RBRACK);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<VarExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(319);
      match(CParser::IDENT);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<NumExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(320);
      match(CParser::NUMBER);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<CharExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(321);
      match(CParser::CHARLIT);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<StringLitExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(322);
      match(CParser::STRINGLIT);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<ParenExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(323);
      match(CParser::LPAREN);
      setState(324);
      expr();
      setState(325);
      match(CParser::RPAREN);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<CastExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(327);
      match(CParser::LPAREN);
      setState(328);
      typeSpec();
      setState(329);
      match(CParser::RPAREN);
      setState(330);
      condExpr(20);
      break;
    }

    case 13: {
      _localctx = _tracker.createInstance<SizeofTypeExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(332);
      match(CParser::SIZEOF_KW);
      setState(333);
      match(CParser::LPAREN);
      setState(334);
      typeSpec();
      setState(335);
      match(CParser::RPAREN);
      break;
    }

    case 14: {
      _localctx = _tracker.createInstance<SizeofExprExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(337);
      match(CParser::SIZEOF_KW);
      setState(338);
      match(CParser::LPAREN);
      setState(339);
      expr();
      setState(340);
      match(CParser::RPAREN);
      break;
    }

    case 15: {
      _localctx = _tracker.createInstance<DerefExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(342);
      match(CParser::STAR);
      setState(343);
      condExpr(17);
      break;
    }

    case 16: {
      _localctx = _tracker.createInstance<AddrOfExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(344);
      match(CParser::AMP);
      setState(345);
      condExpr(16);
      break;
    }

    case 17: {
      _localctx = _tracker.createInstance<UnaryExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(346);
      antlrcpp::downCast<UnaryExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 1879048192) != 0))) {
        antlrcpp::downCast<UnaryExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(347);
      condExpr(15);
      break;
    }

    case 18: {
      _localctx = _tracker.createInstance<PreIncDecVarExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(348);
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
      setState(349);
      match(CParser::IDENT);
      break;
    }

    case 19: {
      _localctx = _tracker.createInstance<PreIncDecIndexExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(350);
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
      setState(351);
      match(CParser::IDENT);
      setState(352);
      match(CParser::LBRACK);
      setState(353);
      antlrcpp::downCast<PreIncDecIndexExprContext *>(_localctx)->idx = expr();
      setState(354);
      match(CParser::RBRACK);
      break;
    }

    case 20: {
      _localctx = _tracker.createInstance<PreIncDecDerefExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(356);
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
      setState(357);
      match(CParser::STAR);
      setState(358);
      antlrcpp::downCast<PreIncDecDerefExprContext *>(_localctx)->ptr = condExpr(12);
      break;
    }

    default:
      break;
    }
    _ctx->stop = _input->LT(-1);
    setState(399);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 43, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        setState(397);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 42, _ctx)) {
        case 1: {
          auto newContext = _tracker.createInstance<MulExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(361);

          if (!(precpred(_ctx, 11))) throw FailedPredicateException(this, "precpred(_ctx, 11)");
          setState(362);
          antlrcpp::downCast<MulExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!((((_la & ~ 0x3fULL) == 0) &&
            ((1ULL << _la) & 6509559808) != 0))) {
            antlrcpp::downCast<MulExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(363);
          condExpr(12);
          break;
        }

        case 2: {
          auto newContext = _tracker.createInstance<AddExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(364);

          if (!(precpred(_ctx, 10))) throw FailedPredicateException(this, "precpred(_ctx, 10)");
          setState(365);
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
          setState(366);
          condExpr(11);
          break;
        }

        case 3: {
          auto newContext = _tracker.createInstance<ShiftExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(367);

          if (!(precpred(_ctx, 9))) throw FailedPredicateException(this, "precpred(_ctx, 9)");
          setState(368);
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
          setState(369);
          condExpr(10);
          break;
        }

        case 4: {
          auto newContext = _tracker.createInstance<RelExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(370);

          if (!(precpred(_ctx, 8))) throw FailedPredicateException(this, "precpred(_ctx, 8)");
          setState(371);
          antlrcpp::downCast<RelExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!((((_la & ~ 0x3fULL) == 0) &&
            ((1ULL << _la) & 1030792151040) != 0))) {
            antlrcpp::downCast<RelExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(372);
          condExpr(9);
          break;
        }

        case 5: {
          auto newContext = _tracker.createInstance<EqExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(373);

          if (!(precpred(_ctx, 7))) throw FailedPredicateException(this, "precpred(_ctx, 7)");
          setState(374);
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
          setState(375);
          condExpr(8);
          break;
        }

        case 6: {
          auto newContext = _tracker.createInstance<BitAndExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(376);

          if (!(precpred(_ctx, 6))) throw FailedPredicateException(this, "precpred(_ctx, 6)");
          setState(377);
          match(CParser::AMP);
          setState(378);
          condExpr(7);
          break;
        }

        case 7: {
          auto newContext = _tracker.createInstance<BitXorExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(379);

          if (!(precpred(_ctx, 5))) throw FailedPredicateException(this, "precpred(_ctx, 5)");
          setState(380);
          match(CParser::CARET);
          setState(381);
          condExpr(6);
          break;
        }

        case 8: {
          auto newContext = _tracker.createInstance<BitOrExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(382);

          if (!(precpred(_ctx, 4))) throw FailedPredicateException(this, "precpred(_ctx, 4)");
          setState(383);
          match(CParser::PIPE);
          setState(384);
          condExpr(5);
          break;
        }

        case 9: {
          auto newContext = _tracker.createInstance<AndExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(385);

          if (!(precpred(_ctx, 3))) throw FailedPredicateException(this, "precpred(_ctx, 3)");
          setState(386);
          match(CParser::ANDAND);
          setState(387);
          condExpr(4);
          break;
        }

        case 10: {
          auto newContext = _tracker.createInstance<OrExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(388);

          if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
          setState(389);
          match(CParser::OROR);
          setState(390);
          condExpr(3);
          break;
        }

        case 11: {
          auto newContext = _tracker.createInstance<TernaryExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          newContext->cond = previousContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(391);

          if (!(precpred(_ctx, 1))) throw FailedPredicateException(this, "precpred(_ctx, 1)");
          setState(392);
          match(CParser::QUESTION);
          setState(393);
          antlrcpp::downCast<TernaryExprContext *>(_localctx)->then = expr();
          setState(394);
          match(CParser::COLON);
          setState(395);
          antlrcpp::downCast<TernaryExprContext *>(_localctx)->els = condExpr(1);
          break;
        }

        default:
          break;
        } 
      }
      setState(401);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 43, _ctx);
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
  enterRule(_localctx, 32, CParser::RuleArgList);
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
    setState(402);
    expr();
    setState(407);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(403);
      match(CParser::COMMA);
      setState(404);
      expr();
      setState(409);
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
    case 15: return condExprSempred(antlrcpp::downCast<CondExprContext *>(context), predicateIndex);

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
