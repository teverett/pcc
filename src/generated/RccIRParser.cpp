
// Generated from /Users/tom/projects/pcc/grammar/RccIRParser.g4 by ANTLR 4.13.2


#include "RccIRParserVisitor.h"

#include "RccIRParser.h"


using namespace antlrcpp;

using namespace antlr4;

namespace {

struct RccIRParserStaticData final {
  RccIRParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  RccIRParserStaticData(const RccIRParserStaticData&) = delete;
  RccIRParserStaticData(RccIRParserStaticData&&) = delete;
  RccIRParserStaticData& operator=(const RccIRParserStaticData&) = delete;
  RccIRParserStaticData& operator=(RccIRParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag rccirparserParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<RccIRParserStaticData> rccirparserParserStaticData = nullptr;

void rccirparserParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (rccirparserParserStaticData != nullptr) {
    return;
  }
#else
  assert(rccirparserParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<RccIRParserStaticData>(
    std::vector<std::string>{
      "program", "typeSpec", "globalDecl", "initList", "initElem", "funcDecl", 
      "typeSpecOrVoid", "paramList", "param", "block", "stmt", "ref", "compoundOp", 
      "expr", "argList", "binOp"
    },
    std::vector<std::string>{
      "", "'; --- globals ---'", "'; --- functions ---'", "'int'", "'char'", 
      "'short'", "'long'", "'unsigned'", "'void'", "'struct'", "'local'", 
      "'if'", "'else'", "'while'", "'return'", "'break'", "'continue'", 
      "'goto'", "'post'", "'oldval'", "'('", "')'", "'['", "']'", "'{'", 
      "", "';'", "','", "'='", "'#'", "'*'", "'&'", "'-'", "'!'", "'~'", 
      "'/'", "'%'", "'+'", "'<<'", "'>>'", "'<='", "'>='", "'<'", "'>'", 
      "'=='", "'!='", "'^'", "'|'", "'&&'", "'||'", "'+='", "'-='", "'*='", 
      "'/='", "'%='", "'&='", "'^='", "'|='", "'<<='", "'>>='", "'\\u003F'", 
      "':'"
    },
    std::vector<std::string>{
      "", "GLOBALS_HEADER", "FUNCTIONS_HEADER", "INT_KW", "CHAR_KW", "SHORT_KW", 
      "LONG_KW", "UNSIGNED_KW", "VOID_KW", "STRUCT_KW", "LOCAL_KW", "IF_KW", 
      "ELSE_KW", "WHILE_KW", "RETURN_KW", "BREAK_KW", "CONTINUE_KW", "GOTO_KW", 
      "POST_KW", "OLDVAL_KW", "LPAREN", "RPAREN", "LBRACK", "RBRACK", "LBRACE", 
      "RBRACE", "SEMI", "COMMA", "ASSIGN", "HASH", "STAR", "AMP", "MINUS", 
      "BANG", "TILDE", "SLASH", "PERCENT", "PLUS", "SHL", "SHR", "LE", "GE", 
      "LT", "GT", "EQ", "NE", "CARET", "PIPE", "ANDAND", "OROR", "PLUSEQ", 
      "MINUSEQ", "STAREQ", "SLASHEQ", "PERCENTEQ", "AMPEQ", "CARETEQ", "PIPEEQ", 
      "SHLEQ", "SHREQ", "QUESTION", "COLON", "IDENT", "NUMBER", "WS", "ASM_BEGIN", 
      "ASM_TEXT", "ASM_END"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,67,316,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,1,0,1,0,5,0,35,8,0,10,0,12,0,38,9,0,1,0,1,0,5,0,42,8,0,10,
  	0,12,0,45,9,0,1,0,1,0,1,1,3,1,50,8,1,1,1,1,1,3,1,54,8,1,1,1,1,1,3,1,58,
  	8,1,1,1,1,1,3,1,62,8,1,1,1,1,1,1,1,3,1,67,8,1,1,1,1,1,1,1,3,1,72,8,1,
  	1,1,5,1,75,8,1,10,1,12,1,78,9,1,1,2,1,2,1,2,1,2,1,2,1,2,1,2,3,2,87,8,
  	2,1,2,1,2,3,2,91,8,2,3,2,93,8,2,1,2,1,2,1,3,1,3,1,3,1,3,5,3,101,8,3,10,
  	3,12,3,104,9,3,3,3,106,8,3,1,3,1,3,1,4,1,4,3,4,112,8,4,1,5,1,5,1,5,1,
  	5,3,5,118,8,5,1,5,1,5,1,5,1,6,1,6,3,6,125,8,6,1,7,1,7,1,7,5,7,130,8,7,
  	10,7,12,7,133,9,7,1,8,1,8,1,8,1,9,1,9,5,9,140,8,9,10,9,12,9,143,9,9,1,
  	9,1,9,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,155,8,10,1,10,1,10,
  	1,10,1,10,1,10,1,10,1,10,3,10,164,8,10,1,10,1,10,3,10,168,8,10,1,10,1,
  	10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,183,8,
  	10,1,10,1,10,3,10,187,8,10,3,10,189,8,10,1,10,1,10,1,10,1,10,1,10,1,10,
  	1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,205,8,10,1,11,1,11,1,11,
  	1,11,1,12,1,12,1,13,1,13,3,13,215,8,13,1,13,1,13,1,13,1,13,1,13,1,13,
  	1,13,1,13,3,13,225,8,13,1,13,1,13,1,13,1,13,1,13,3,13,232,8,13,1,13,1,
  	13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,247,8,
  	13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,259,8,13,1,
  	13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,
  	13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,
  	13,3,13,290,8,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,
  	13,1,13,3,13,304,8,13,1,14,1,14,1,14,5,14,309,8,14,10,14,12,14,312,9,
  	14,1,15,1,15,1,15,0,0,16,0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,0,
  	3,1,0,50,59,1,0,31,34,2,0,30,32,35,49,360,0,32,1,0,0,0,2,71,1,0,0,0,4,
  	79,1,0,0,0,6,96,1,0,0,0,8,111,1,0,0,0,10,113,1,0,0,0,12,124,1,0,0,0,14,
  	126,1,0,0,0,16,134,1,0,0,0,18,137,1,0,0,0,20,204,1,0,0,0,22,206,1,0,0,
  	0,24,210,1,0,0,0,26,303,1,0,0,0,28,305,1,0,0,0,30,313,1,0,0,0,32,36,5,
  	1,0,0,33,35,3,4,2,0,34,33,1,0,0,0,35,38,1,0,0,0,36,34,1,0,0,0,36,37,1,
  	0,0,0,37,39,1,0,0,0,38,36,1,0,0,0,39,43,5,2,0,0,40,42,3,10,5,0,41,40,
  	1,0,0,0,42,45,1,0,0,0,43,41,1,0,0,0,43,44,1,0,0,0,44,46,1,0,0,0,45,43,
  	1,0,0,0,46,47,5,0,0,1,47,1,1,0,0,0,48,50,5,7,0,0,49,48,1,0,0,0,49,50,
  	1,0,0,0,50,51,1,0,0,0,51,72,5,4,0,0,52,54,5,7,0,0,53,52,1,0,0,0,53,54,
  	1,0,0,0,54,55,1,0,0,0,55,72,5,5,0,0,56,58,5,7,0,0,57,56,1,0,0,0,57,58,
  	1,0,0,0,58,59,1,0,0,0,59,72,5,3,0,0,60,62,5,7,0,0,61,60,1,0,0,0,61,62,
  	1,0,0,0,62,63,1,0,0,0,63,64,5,6,0,0,64,72,5,6,0,0,65,67,5,7,0,0,66,65,
  	1,0,0,0,66,67,1,0,0,0,67,68,1,0,0,0,68,72,5,6,0,0,69,70,5,9,0,0,70,72,
  	5,62,0,0,71,49,1,0,0,0,71,53,1,0,0,0,71,57,1,0,0,0,71,61,1,0,0,0,71,66,
  	1,0,0,0,71,69,1,0,0,0,72,76,1,0,0,0,73,75,5,30,0,0,74,73,1,0,0,0,75,78,
  	1,0,0,0,76,74,1,0,0,0,76,77,1,0,0,0,77,3,1,0,0,0,78,76,1,0,0,0,79,80,
  	3,2,1,0,80,92,3,22,11,0,81,82,5,22,0,0,82,83,5,63,0,0,83,86,5,23,0,0,
  	84,85,5,28,0,0,85,87,3,6,3,0,86,84,1,0,0,0,86,87,1,0,0,0,87,93,1,0,0,
  	0,88,89,5,28,0,0,89,91,5,63,0,0,90,88,1,0,0,0,90,91,1,0,0,0,91,93,1,0,
  	0,0,92,81,1,0,0,0,92,90,1,0,0,0,93,94,1,0,0,0,94,95,5,26,0,0,95,5,1,0,
  	0,0,96,105,5,24,0,0,97,102,3,8,4,0,98,99,5,27,0,0,99,101,3,8,4,0,100,
  	98,1,0,0,0,101,104,1,0,0,0,102,100,1,0,0,0,102,103,1,0,0,0,103,106,1,
  	0,0,0,104,102,1,0,0,0,105,97,1,0,0,0,105,106,1,0,0,0,106,107,1,0,0,0,
  	107,108,5,25,0,0,108,7,1,0,0,0,109,112,5,63,0,0,110,112,3,22,11,0,111,
  	109,1,0,0,0,111,110,1,0,0,0,112,9,1,0,0,0,113,114,3,12,6,0,114,115,3,
  	22,11,0,115,117,5,20,0,0,116,118,3,14,7,0,117,116,1,0,0,0,117,118,1,0,
  	0,0,118,119,1,0,0,0,119,120,5,21,0,0,120,121,3,18,9,0,121,11,1,0,0,0,
  	122,125,3,2,1,0,123,125,5,8,0,0,124,122,1,0,0,0,124,123,1,0,0,0,125,13,
  	1,0,0,0,126,131,3,16,8,0,127,128,5,27,0,0,128,130,3,16,8,0,129,127,1,
  	0,0,0,130,133,1,0,0,0,131,129,1,0,0,0,131,132,1,0,0,0,132,15,1,0,0,0,
  	133,131,1,0,0,0,134,135,3,2,1,0,135,136,3,22,11,0,136,17,1,0,0,0,137,
  	141,5,24,0,0,138,140,3,20,10,0,139,138,1,0,0,0,140,143,1,0,0,0,141,139,
  	1,0,0,0,141,142,1,0,0,0,142,144,1,0,0,0,143,141,1,0,0,0,144,145,5,25,
  	0,0,145,19,1,0,0,0,146,205,3,18,9,0,147,148,5,11,0,0,148,149,5,20,0,0,
  	149,150,3,26,13,0,150,151,5,21,0,0,151,154,3,20,10,0,152,153,5,12,0,0,
  	153,155,3,20,10,0,154,152,1,0,0,0,154,155,1,0,0,0,155,205,1,0,0,0,156,
  	157,5,13,0,0,157,158,5,20,0,0,158,159,3,26,13,0,159,160,5,21,0,0,160,
  	163,3,20,10,0,161,162,5,18,0,0,162,164,3,20,10,0,163,161,1,0,0,0,163,
  	164,1,0,0,0,164,205,1,0,0,0,165,167,5,14,0,0,166,168,3,26,13,0,167,166,
  	1,0,0,0,167,168,1,0,0,0,168,169,1,0,0,0,169,205,5,26,0,0,170,171,5,15,
  	0,0,171,205,5,26,0,0,172,173,5,16,0,0,173,205,5,26,0,0,174,175,5,10,0,
  	0,175,176,3,2,1,0,176,188,3,22,11,0,177,178,5,22,0,0,178,179,5,63,0,0,
  	179,182,5,23,0,0,180,181,5,28,0,0,181,183,3,6,3,0,182,180,1,0,0,0,182,
  	183,1,0,0,0,183,189,1,0,0,0,184,185,5,28,0,0,185,187,3,26,13,0,186,184,
  	1,0,0,0,186,187,1,0,0,0,187,189,1,0,0,0,188,177,1,0,0,0,188,186,1,0,0,
  	0,189,190,1,0,0,0,190,191,5,26,0,0,191,205,1,0,0,0,192,193,5,65,0,0,193,
  	194,5,66,0,0,194,205,5,67,0,0,195,205,5,26,0,0,196,197,5,62,0,0,197,205,
  	5,61,0,0,198,199,5,17,0,0,199,200,5,62,0,0,200,205,5,26,0,0,201,202,3,
  	26,13,0,202,203,5,26,0,0,203,205,1,0,0,0,204,146,1,0,0,0,204,147,1,0,
  	0,0,204,156,1,0,0,0,204,165,1,0,0,0,204,170,1,0,0,0,204,172,1,0,0,0,204,
  	174,1,0,0,0,204,192,1,0,0,0,204,195,1,0,0,0,204,196,1,0,0,0,204,198,1,
  	0,0,0,204,201,1,0,0,0,205,21,1,0,0,0,206,207,5,62,0,0,207,208,5,29,0,
  	0,208,209,5,63,0,0,209,23,1,0,0,0,210,211,7,0,0,0,211,25,1,0,0,0,212,
  	304,5,63,0,0,213,215,5,19,0,0,214,213,1,0,0,0,214,215,1,0,0,0,215,216,
  	1,0,0,0,216,217,3,22,11,0,217,218,5,22,0,0,218,219,3,26,13,0,219,220,
  	5,23,0,0,220,221,3,24,12,0,221,222,3,26,13,0,222,304,1,0,0,0,223,225,
  	5,19,0,0,224,223,1,0,0,0,224,225,1,0,0,0,225,226,1,0,0,0,226,227,3,22,
  	11,0,227,228,3,24,12,0,228,229,3,26,13,0,229,304,1,0,0,0,230,232,5,19,
  	0,0,231,230,1,0,0,0,231,232,1,0,0,0,232,233,1,0,0,0,233,234,5,30,0,0,
  	234,235,5,20,0,0,235,236,3,26,13,0,236,237,5,21,0,0,237,238,3,24,12,0,
  	238,239,3,26,13,0,239,304,1,0,0,0,240,241,3,22,11,0,241,242,5,22,0,0,
  	242,243,3,26,13,0,243,246,5,23,0,0,244,245,5,28,0,0,245,247,3,26,13,0,
  	246,244,1,0,0,0,246,247,1,0,0,0,247,304,1,0,0,0,248,249,3,22,11,0,249,
  	250,5,28,0,0,250,251,3,26,13,0,251,304,1,0,0,0,252,253,5,30,0,0,253,254,
  	5,20,0,0,254,255,3,26,13,0,255,258,5,21,0,0,256,257,5,28,0,0,257,259,
  	3,26,13,0,258,256,1,0,0,0,258,259,1,0,0,0,259,304,1,0,0,0,260,261,7,1,
  	0,0,261,262,5,20,0,0,262,263,3,26,13,0,263,264,5,21,0,0,264,304,1,0,0,
  	0,265,266,5,20,0,0,266,267,3,26,13,0,267,268,3,30,15,0,268,269,3,26,13,
  	0,269,270,5,21,0,0,270,304,1,0,0,0,271,272,5,20,0,0,272,273,3,26,13,0,
  	273,274,5,60,0,0,274,275,3,26,13,0,275,276,5,61,0,0,276,277,3,26,13,0,
  	277,278,5,21,0,0,278,304,1,0,0,0,279,280,5,20,0,0,280,281,3,2,1,0,281,
  	282,5,21,0,0,282,283,5,20,0,0,283,284,3,26,13,0,284,285,5,21,0,0,285,
  	304,1,0,0,0,286,287,3,22,11,0,287,289,5,20,0,0,288,290,3,28,14,0,289,
  	288,1,0,0,0,289,290,1,0,0,0,290,291,1,0,0,0,291,292,5,21,0,0,292,304,
  	1,0,0,0,293,294,5,20,0,0,294,295,3,26,13,0,295,296,5,21,0,0,296,297,5,
  	20,0,0,297,298,5,21,0,0,298,304,1,0,0,0,299,300,5,62,0,0,300,301,5,20,
  	0,0,301,304,5,21,0,0,302,304,3,22,11,0,303,212,1,0,0,0,303,214,1,0,0,
  	0,303,224,1,0,0,0,303,231,1,0,0,0,303,240,1,0,0,0,303,248,1,0,0,0,303,
  	252,1,0,0,0,303,260,1,0,0,0,303,265,1,0,0,0,303,271,1,0,0,0,303,279,1,
  	0,0,0,303,286,1,0,0,0,303,293,1,0,0,0,303,299,1,0,0,0,303,302,1,0,0,0,
  	304,27,1,0,0,0,305,310,3,26,13,0,306,307,5,27,0,0,307,309,3,26,13,0,308,
  	306,1,0,0,0,309,312,1,0,0,0,310,308,1,0,0,0,310,311,1,0,0,0,311,29,1,
  	0,0,0,312,310,1,0,0,0,313,314,7,2,0,0,314,31,1,0,0,0,34,36,43,49,53,57,
  	61,66,71,76,86,90,92,102,105,111,117,124,131,141,154,163,167,182,186,
  	188,204,214,224,231,246,258,289,303,310
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  rccirparserParserStaticData = std::move(staticData);
}

}

RccIRParser::RccIRParser(TokenStream *input) : RccIRParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

RccIRParser::RccIRParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  RccIRParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *rccirparserParserStaticData->atn, rccirparserParserStaticData->decisionToDFA, rccirparserParserStaticData->sharedContextCache, options);
}

RccIRParser::~RccIRParser() {
  delete _interpreter;
}

const atn::ATN& RccIRParser::getATN() const {
  return *rccirparserParserStaticData->atn;
}

std::string RccIRParser::getGrammarFileName() const {
  return "RccIRParser.g4";
}

const std::vector<std::string>& RccIRParser::getRuleNames() const {
  return rccirparserParserStaticData->ruleNames;
}

const dfa::Vocabulary& RccIRParser::getVocabulary() const {
  return rccirparserParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView RccIRParser::getSerializedATN() const {
  return rccirparserParserStaticData->serializedATN;
}


//----------------- ProgramContext ------------------------------------------------------------------

RccIRParser::ProgramContext::ProgramContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccIRParser::ProgramContext::GLOBALS_HEADER() {
  return getToken(RccIRParser::GLOBALS_HEADER, 0);
}

tree::TerminalNode* RccIRParser::ProgramContext::FUNCTIONS_HEADER() {
  return getToken(RccIRParser::FUNCTIONS_HEADER, 0);
}

tree::TerminalNode* RccIRParser::ProgramContext::EOF() {
  return getToken(RccIRParser::EOF, 0);
}

std::vector<RccIRParser::GlobalDeclContext *> RccIRParser::ProgramContext::globalDecl() {
  return getRuleContexts<RccIRParser::GlobalDeclContext>();
}

RccIRParser::GlobalDeclContext* RccIRParser::ProgramContext::globalDecl(size_t i) {
  return getRuleContext<RccIRParser::GlobalDeclContext>(i);
}

std::vector<RccIRParser::FuncDeclContext *> RccIRParser::ProgramContext::funcDecl() {
  return getRuleContexts<RccIRParser::FuncDeclContext>();
}

RccIRParser::FuncDeclContext* RccIRParser::ProgramContext::funcDecl(size_t i) {
  return getRuleContext<RccIRParser::FuncDeclContext>(i);
}


size_t RccIRParser::ProgramContext::getRuleIndex() const {
  return RccIRParser::RuleProgram;
}


std::any RccIRParser::ProgramContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitProgram(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::ProgramContext* RccIRParser::program() {
  ProgramContext *_localctx = _tracker.createInstance<ProgramContext>(_ctx, getState());
  enterRule(_localctx, 0, RccIRParser::RuleProgram);
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
    setState(32);
    match(RccIRParser::GLOBALS_HEADER);
    setState(36);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 760) != 0)) {
      setState(33);
      globalDecl();
      setState(38);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(39);
    match(RccIRParser::FUNCTIONS_HEADER);
    setState(43);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1016) != 0)) {
      setState(40);
      funcDecl();
      setState(45);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(46);
    match(RccIRParser::EOF);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TypeSpecContext ------------------------------------------------------------------

RccIRParser::TypeSpecContext::TypeSpecContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccIRParser::TypeSpecContext::CHAR_KW() {
  return getToken(RccIRParser::CHAR_KW, 0);
}

tree::TerminalNode* RccIRParser::TypeSpecContext::SHORT_KW() {
  return getToken(RccIRParser::SHORT_KW, 0);
}

tree::TerminalNode* RccIRParser::TypeSpecContext::INT_KW() {
  return getToken(RccIRParser::INT_KW, 0);
}

std::vector<tree::TerminalNode *> RccIRParser::TypeSpecContext::LONG_KW() {
  return getTokens(RccIRParser::LONG_KW);
}

tree::TerminalNode* RccIRParser::TypeSpecContext::LONG_KW(size_t i) {
  return getToken(RccIRParser::LONG_KW, i);
}

tree::TerminalNode* RccIRParser::TypeSpecContext::STRUCT_KW() {
  return getToken(RccIRParser::STRUCT_KW, 0);
}

tree::TerminalNode* RccIRParser::TypeSpecContext::IDENT() {
  return getToken(RccIRParser::IDENT, 0);
}

std::vector<tree::TerminalNode *> RccIRParser::TypeSpecContext::STAR() {
  return getTokens(RccIRParser::STAR);
}

tree::TerminalNode* RccIRParser::TypeSpecContext::STAR(size_t i) {
  return getToken(RccIRParser::STAR, i);
}

tree::TerminalNode* RccIRParser::TypeSpecContext::UNSIGNED_KW() {
  return getToken(RccIRParser::UNSIGNED_KW, 0);
}


size_t RccIRParser::TypeSpecContext::getRuleIndex() const {
  return RccIRParser::RuleTypeSpec;
}


std::any RccIRParser::TypeSpecContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitTypeSpec(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::TypeSpecContext* RccIRParser::typeSpec() {
  TypeSpecContext *_localctx = _tracker.createInstance<TypeSpecContext>(_ctx, getState());
  enterRule(_localctx, 2, RccIRParser::RuleTypeSpec);
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
    setState(71);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 7, _ctx)) {
    case 1: {
      setState(49);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::UNSIGNED_KW) {
        setState(48);
        match(RccIRParser::UNSIGNED_KW);
      }
      setState(51);
      match(RccIRParser::CHAR_KW);
      break;
    }

    case 2: {
      setState(53);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::UNSIGNED_KW) {
        setState(52);
        match(RccIRParser::UNSIGNED_KW);
      }
      setState(55);
      match(RccIRParser::SHORT_KW);
      break;
    }

    case 3: {
      setState(57);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::UNSIGNED_KW) {
        setState(56);
        match(RccIRParser::UNSIGNED_KW);
      }
      setState(59);
      match(RccIRParser::INT_KW);
      break;
    }

    case 4: {
      setState(61);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::UNSIGNED_KW) {
        setState(60);
        match(RccIRParser::UNSIGNED_KW);
      }
      setState(63);
      match(RccIRParser::LONG_KW);
      setState(64);
      match(RccIRParser::LONG_KW);
      break;
    }

    case 5: {
      setState(66);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::UNSIGNED_KW) {
        setState(65);
        match(RccIRParser::UNSIGNED_KW);
      }
      setState(68);
      match(RccIRParser::LONG_KW);
      break;
    }

    case 6: {
      setState(69);
      match(RccIRParser::STRUCT_KW);
      setState(70);
      match(RccIRParser::IDENT);
      break;
    }

    default:
      break;
    }
    setState(76);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == RccIRParser::STAR) {
      setState(73);
      match(RccIRParser::STAR);
      setState(78);
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

//----------------- GlobalDeclContext ------------------------------------------------------------------

RccIRParser::GlobalDeclContext::GlobalDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

RccIRParser::TypeSpecContext* RccIRParser::GlobalDeclContext::typeSpec() {
  return getRuleContext<RccIRParser::TypeSpecContext>(0);
}

RccIRParser::RefContext* RccIRParser::GlobalDeclContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

tree::TerminalNode* RccIRParser::GlobalDeclContext::SEMI() {
  return getToken(RccIRParser::SEMI, 0);
}

tree::TerminalNode* RccIRParser::GlobalDeclContext::LBRACK() {
  return getToken(RccIRParser::LBRACK, 0);
}

tree::TerminalNode* RccIRParser::GlobalDeclContext::NUMBER() {
  return getToken(RccIRParser::NUMBER, 0);
}

tree::TerminalNode* RccIRParser::GlobalDeclContext::RBRACK() {
  return getToken(RccIRParser::RBRACK, 0);
}

tree::TerminalNode* RccIRParser::GlobalDeclContext::ASSIGN() {
  return getToken(RccIRParser::ASSIGN, 0);
}

RccIRParser::InitListContext* RccIRParser::GlobalDeclContext::initList() {
  return getRuleContext<RccIRParser::InitListContext>(0);
}


size_t RccIRParser::GlobalDeclContext::getRuleIndex() const {
  return RccIRParser::RuleGlobalDecl;
}


std::any RccIRParser::GlobalDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitGlobalDecl(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::GlobalDeclContext* RccIRParser::globalDecl() {
  GlobalDeclContext *_localctx = _tracker.createInstance<GlobalDeclContext>(_ctx, getState());
  enterRule(_localctx, 4, RccIRParser::RuleGlobalDecl);
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
    setState(79);
    typeSpec();
    setState(80);
    ref();
    setState(92);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case RccIRParser::LBRACK: {
        setState(81);
        match(RccIRParser::LBRACK);
        setState(82);
        match(RccIRParser::NUMBER);
        setState(83);
        match(RccIRParser::RBRACK);
        setState(86);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == RccIRParser::ASSIGN) {
          setState(84);
          match(RccIRParser::ASSIGN);
          setState(85);
          initList();
        }
        break;
      }

      case RccIRParser::SEMI:
      case RccIRParser::ASSIGN: {
        setState(90);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == RccIRParser::ASSIGN) {
          setState(88);
          match(RccIRParser::ASSIGN);
          setState(89);
          match(RccIRParser::NUMBER);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(94);
    match(RccIRParser::SEMI);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- InitListContext ------------------------------------------------------------------

RccIRParser::InitListContext::InitListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccIRParser::InitListContext::LBRACE() {
  return getToken(RccIRParser::LBRACE, 0);
}

tree::TerminalNode* RccIRParser::InitListContext::RBRACE() {
  return getToken(RccIRParser::RBRACE, 0);
}

std::vector<RccIRParser::InitElemContext *> RccIRParser::InitListContext::initElem() {
  return getRuleContexts<RccIRParser::InitElemContext>();
}

RccIRParser::InitElemContext* RccIRParser::InitListContext::initElem(size_t i) {
  return getRuleContext<RccIRParser::InitElemContext>(i);
}

std::vector<tree::TerminalNode *> RccIRParser::InitListContext::COMMA() {
  return getTokens(RccIRParser::COMMA);
}

tree::TerminalNode* RccIRParser::InitListContext::COMMA(size_t i) {
  return getToken(RccIRParser::COMMA, i);
}


size_t RccIRParser::InitListContext::getRuleIndex() const {
  return RccIRParser::RuleInitList;
}


std::any RccIRParser::InitListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitInitList(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::InitListContext* RccIRParser::initList() {
  InitListContext *_localctx = _tracker.createInstance<InitListContext>(_ctx, getState());
  enterRule(_localctx, 6, RccIRParser::RuleInitList);
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
    setState(96);
    match(RccIRParser::LBRACE);
    setState(105);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == RccIRParser::IDENT

    || _la == RccIRParser::NUMBER) {
      setState(97);
      initElem();
      setState(102);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == RccIRParser::COMMA) {
        setState(98);
        match(RccIRParser::COMMA);
        setState(99);
        initElem();
        setState(104);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(107);
    match(RccIRParser::RBRACE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- InitElemContext ------------------------------------------------------------------

RccIRParser::InitElemContext::InitElemContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccIRParser::InitElemContext::NUMBER() {
  return getToken(RccIRParser::NUMBER, 0);
}

RccIRParser::RefContext* RccIRParser::InitElemContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}


size_t RccIRParser::InitElemContext::getRuleIndex() const {
  return RccIRParser::RuleInitElem;
}


std::any RccIRParser::InitElemContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitInitElem(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::InitElemContext* RccIRParser::initElem() {
  InitElemContext *_localctx = _tracker.createInstance<InitElemContext>(_ctx, getState());
  enterRule(_localctx, 8, RccIRParser::RuleInitElem);

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
      case RccIRParser::NUMBER: {
        enterOuterAlt(_localctx, 1);
        setState(109);
        match(RccIRParser::NUMBER);
        break;
      }

      case RccIRParser::IDENT: {
        enterOuterAlt(_localctx, 2);
        setState(110);
        ref();
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

//----------------- FuncDeclContext ------------------------------------------------------------------

RccIRParser::FuncDeclContext::FuncDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

RccIRParser::TypeSpecOrVoidContext* RccIRParser::FuncDeclContext::typeSpecOrVoid() {
  return getRuleContext<RccIRParser::TypeSpecOrVoidContext>(0);
}

RccIRParser::RefContext* RccIRParser::FuncDeclContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

tree::TerminalNode* RccIRParser::FuncDeclContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

tree::TerminalNode* RccIRParser::FuncDeclContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

RccIRParser::BlockContext* RccIRParser::FuncDeclContext::block() {
  return getRuleContext<RccIRParser::BlockContext>(0);
}

RccIRParser::ParamListContext* RccIRParser::FuncDeclContext::paramList() {
  return getRuleContext<RccIRParser::ParamListContext>(0);
}


size_t RccIRParser::FuncDeclContext::getRuleIndex() const {
  return RccIRParser::RuleFuncDecl;
}


std::any RccIRParser::FuncDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitFuncDecl(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::FuncDeclContext* RccIRParser::funcDecl() {
  FuncDeclContext *_localctx = _tracker.createInstance<FuncDeclContext>(_ctx, getState());
  enterRule(_localctx, 10, RccIRParser::RuleFuncDecl);
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
    typeSpecOrVoid();
    setState(114);
    ref();
    setState(115);
    match(RccIRParser::LPAREN);
    setState(117);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 760) != 0)) {
      setState(116);
      paramList();
    }
    setState(119);
    match(RccIRParser::RPAREN);
    setState(120);
    block();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TypeSpecOrVoidContext ------------------------------------------------------------------

RccIRParser::TypeSpecOrVoidContext::TypeSpecOrVoidContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

RccIRParser::TypeSpecContext* RccIRParser::TypeSpecOrVoidContext::typeSpec() {
  return getRuleContext<RccIRParser::TypeSpecContext>(0);
}

tree::TerminalNode* RccIRParser::TypeSpecOrVoidContext::VOID_KW() {
  return getToken(RccIRParser::VOID_KW, 0);
}


size_t RccIRParser::TypeSpecOrVoidContext::getRuleIndex() const {
  return RccIRParser::RuleTypeSpecOrVoid;
}


std::any RccIRParser::TypeSpecOrVoidContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitTypeSpecOrVoid(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::TypeSpecOrVoidContext* RccIRParser::typeSpecOrVoid() {
  TypeSpecOrVoidContext *_localctx = _tracker.createInstance<TypeSpecOrVoidContext>(_ctx, getState());
  enterRule(_localctx, 12, RccIRParser::RuleTypeSpecOrVoid);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(124);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case RccIRParser::INT_KW:
      case RccIRParser::CHAR_KW:
      case RccIRParser::SHORT_KW:
      case RccIRParser::LONG_KW:
      case RccIRParser::UNSIGNED_KW:
      case RccIRParser::STRUCT_KW: {
        enterOuterAlt(_localctx, 1);
        setState(122);
        typeSpec();
        break;
      }

      case RccIRParser::VOID_KW: {
        enterOuterAlt(_localctx, 2);
        setState(123);
        match(RccIRParser::VOID_KW);
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

RccIRParser::ParamListContext::ParamListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<RccIRParser::ParamContext *> RccIRParser::ParamListContext::param() {
  return getRuleContexts<RccIRParser::ParamContext>();
}

RccIRParser::ParamContext* RccIRParser::ParamListContext::param(size_t i) {
  return getRuleContext<RccIRParser::ParamContext>(i);
}

std::vector<tree::TerminalNode *> RccIRParser::ParamListContext::COMMA() {
  return getTokens(RccIRParser::COMMA);
}

tree::TerminalNode* RccIRParser::ParamListContext::COMMA(size_t i) {
  return getToken(RccIRParser::COMMA, i);
}


size_t RccIRParser::ParamListContext::getRuleIndex() const {
  return RccIRParser::RuleParamList;
}


std::any RccIRParser::ParamListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitParamList(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::ParamListContext* RccIRParser::paramList() {
  ParamListContext *_localctx = _tracker.createInstance<ParamListContext>(_ctx, getState());
  enterRule(_localctx, 14, RccIRParser::RuleParamList);
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
    setState(126);
    param();
    setState(131);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == RccIRParser::COMMA) {
      setState(127);
      match(RccIRParser::COMMA);
      setState(128);
      param();
      setState(133);
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

RccIRParser::ParamContext::ParamContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

RccIRParser::TypeSpecContext* RccIRParser::ParamContext::typeSpec() {
  return getRuleContext<RccIRParser::TypeSpecContext>(0);
}

RccIRParser::RefContext* RccIRParser::ParamContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}


size_t RccIRParser::ParamContext::getRuleIndex() const {
  return RccIRParser::RuleParam;
}


std::any RccIRParser::ParamContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitParam(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::ParamContext* RccIRParser::param() {
  ParamContext *_localctx = _tracker.createInstance<ParamContext>(_ctx, getState());
  enterRule(_localctx, 16, RccIRParser::RuleParam);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(134);
    typeSpec();
    setState(135);
    ref();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BlockContext ------------------------------------------------------------------

RccIRParser::BlockContext::BlockContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccIRParser::BlockContext::LBRACE() {
  return getToken(RccIRParser::LBRACE, 0);
}

tree::TerminalNode* RccIRParser::BlockContext::RBRACE() {
  return getToken(RccIRParser::RBRACE, 0);
}

std::vector<RccIRParser::StmtContext *> RccIRParser::BlockContext::stmt() {
  return getRuleContexts<RccIRParser::StmtContext>();
}

RccIRParser::StmtContext* RccIRParser::BlockContext::stmt(size_t i) {
  return getRuleContext<RccIRParser::StmtContext>(i);
}


size_t RccIRParser::BlockContext::getRuleIndex() const {
  return RccIRParser::RuleBlock;
}


std::any RccIRParser::BlockContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitBlock(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::BlockContext* RccIRParser::block() {
  BlockContext *_localctx = _tracker.createInstance<BlockContext>(_ctx, getState());
  enterRule(_localctx, 18, RccIRParser::RuleBlock);
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
    setState(137);
    match(RccIRParser::LBRACE);
    setState(141);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 10) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 10)) & 49539595933665019) != 0)) {
      setState(138);
      stmt();
      setState(143);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(144);
    match(RccIRParser::RBRACE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StmtContext ------------------------------------------------------------------

RccIRParser::StmtContext::StmtContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t RccIRParser::StmtContext::getRuleIndex() const {
  return RccIRParser::RuleStmt;
}

void RccIRParser::StmtContext::copyFrom(StmtContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- LocalDeclStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::LocalDeclStmtContext::LOCAL_KW() {
  return getToken(RccIRParser::LOCAL_KW, 0);
}

RccIRParser::TypeSpecContext* RccIRParser::LocalDeclStmtContext::typeSpec() {
  return getRuleContext<RccIRParser::TypeSpecContext>(0);
}

RccIRParser::RefContext* RccIRParser::LocalDeclStmtContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

tree::TerminalNode* RccIRParser::LocalDeclStmtContext::SEMI() {
  return getToken(RccIRParser::SEMI, 0);
}

tree::TerminalNode* RccIRParser::LocalDeclStmtContext::LBRACK() {
  return getToken(RccIRParser::LBRACK, 0);
}

tree::TerminalNode* RccIRParser::LocalDeclStmtContext::NUMBER() {
  return getToken(RccIRParser::NUMBER, 0);
}

tree::TerminalNode* RccIRParser::LocalDeclStmtContext::RBRACK() {
  return getToken(RccIRParser::RBRACK, 0);
}

tree::TerminalNode* RccIRParser::LocalDeclStmtContext::ASSIGN() {
  return getToken(RccIRParser::ASSIGN, 0);
}

RccIRParser::InitListContext* RccIRParser::LocalDeclStmtContext::initList() {
  return getRuleContext<RccIRParser::InitListContext>(0);
}

RccIRParser::ExprContext* RccIRParser::LocalDeclStmtContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

RccIRParser::LocalDeclStmtContext::LocalDeclStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::LocalDeclStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitLocalDeclStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AsmStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::AsmStmtContext::ASM_BEGIN() {
  return getToken(RccIRParser::ASM_BEGIN, 0);
}

tree::TerminalNode* RccIRParser::AsmStmtContext::ASM_TEXT() {
  return getToken(RccIRParser::ASM_TEXT, 0);
}

tree::TerminalNode* RccIRParser::AsmStmtContext::ASM_END() {
  return getToken(RccIRParser::ASM_END, 0);
}

RccIRParser::AsmStmtContext::AsmStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::AsmStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitAsmStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ExprStmtContext ------------------------------------------------------------------

RccIRParser::ExprContext* RccIRParser::ExprStmtContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

tree::TerminalNode* RccIRParser::ExprStmtContext::SEMI() {
  return getToken(RccIRParser::SEMI, 0);
}

RccIRParser::ExprStmtContext::ExprStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::ExprStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitExprStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- WhileStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::WhileStmtContext::WHILE_KW() {
  return getToken(RccIRParser::WHILE_KW, 0);
}

tree::TerminalNode* RccIRParser::WhileStmtContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

RccIRParser::ExprContext* RccIRParser::WhileStmtContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

tree::TerminalNode* RccIRParser::WhileStmtContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

std::vector<RccIRParser::StmtContext *> RccIRParser::WhileStmtContext::stmt() {
  return getRuleContexts<RccIRParser::StmtContext>();
}

RccIRParser::StmtContext* RccIRParser::WhileStmtContext::stmt(size_t i) {
  return getRuleContext<RccIRParser::StmtContext>(i);
}

tree::TerminalNode* RccIRParser::WhileStmtContext::POST_KW() {
  return getToken(RccIRParser::POST_KW, 0);
}

RccIRParser::WhileStmtContext::WhileStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::WhileStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitWhileStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IfStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::IfStmtContext::IF_KW() {
  return getToken(RccIRParser::IF_KW, 0);
}

tree::TerminalNode* RccIRParser::IfStmtContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

RccIRParser::ExprContext* RccIRParser::IfStmtContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

tree::TerminalNode* RccIRParser::IfStmtContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

std::vector<RccIRParser::StmtContext *> RccIRParser::IfStmtContext::stmt() {
  return getRuleContexts<RccIRParser::StmtContext>();
}

RccIRParser::StmtContext* RccIRParser::IfStmtContext::stmt(size_t i) {
  return getRuleContext<RccIRParser::StmtContext>(i);
}

tree::TerminalNode* RccIRParser::IfStmtContext::ELSE_KW() {
  return getToken(RccIRParser::ELSE_KW, 0);
}

RccIRParser::IfStmtContext::IfStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::IfStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitIfStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BlockStmtContext ------------------------------------------------------------------

RccIRParser::BlockContext* RccIRParser::BlockStmtContext::block() {
  return getRuleContext<RccIRParser::BlockContext>(0);
}

RccIRParser::BlockStmtContext::BlockStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::BlockStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitBlockStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BreakStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::BreakStmtContext::BREAK_KW() {
  return getToken(RccIRParser::BREAK_KW, 0);
}

tree::TerminalNode* RccIRParser::BreakStmtContext::SEMI() {
  return getToken(RccIRParser::SEMI, 0);
}

RccIRParser::BreakStmtContext::BreakStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::BreakStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitBreakStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- EmptyStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::EmptyStmtContext::SEMI() {
  return getToken(RccIRParser::SEMI, 0);
}

RccIRParser::EmptyStmtContext::EmptyStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::EmptyStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitEmptyStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- GotoStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::GotoStmtContext::GOTO_KW() {
  return getToken(RccIRParser::GOTO_KW, 0);
}

tree::TerminalNode* RccIRParser::GotoStmtContext::IDENT() {
  return getToken(RccIRParser::IDENT, 0);
}

tree::TerminalNode* RccIRParser::GotoStmtContext::SEMI() {
  return getToken(RccIRParser::SEMI, 0);
}

RccIRParser::GotoStmtContext::GotoStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::GotoStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitGotoStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LabelStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::LabelStmtContext::IDENT() {
  return getToken(RccIRParser::IDENT, 0);
}

tree::TerminalNode* RccIRParser::LabelStmtContext::COLON() {
  return getToken(RccIRParser::COLON, 0);
}

RccIRParser::LabelStmtContext::LabelStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::LabelStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitLabelStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ReturnStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::ReturnStmtContext::RETURN_KW() {
  return getToken(RccIRParser::RETURN_KW, 0);
}

tree::TerminalNode* RccIRParser::ReturnStmtContext::SEMI() {
  return getToken(RccIRParser::SEMI, 0);
}

RccIRParser::ExprContext* RccIRParser::ReturnStmtContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

RccIRParser::ReturnStmtContext::ReturnStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::ReturnStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitReturnStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ContinueStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::ContinueStmtContext::CONTINUE_KW() {
  return getToken(RccIRParser::CONTINUE_KW, 0);
}

tree::TerminalNode* RccIRParser::ContinueStmtContext::SEMI() {
  return getToken(RccIRParser::SEMI, 0);
}

RccIRParser::ContinueStmtContext::ContinueStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::ContinueStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitContinueStmt(this);
  else
    return visitor->visitChildren(this);
}
RccIRParser::StmtContext* RccIRParser::stmt() {
  StmtContext *_localctx = _tracker.createInstance<StmtContext>(_ctx, getState());
  enterRule(_localctx, 20, RccIRParser::RuleStmt);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(204);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 25, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<RccIRParser::BlockStmtContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(146);
      block();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<RccIRParser::IfStmtContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(147);
      match(RccIRParser::IF_KW);
      setState(148);
      match(RccIRParser::LPAREN);
      setState(149);
      expr();
      setState(150);
      match(RccIRParser::RPAREN);
      setState(151);
      stmt();
      setState(154);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 19, _ctx)) {
      case 1: {
        setState(152);
        match(RccIRParser::ELSE_KW);
        setState(153);
        stmt();
        break;
      }

      default:
        break;
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<RccIRParser::WhileStmtContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(156);
      match(RccIRParser::WHILE_KW);
      setState(157);
      match(RccIRParser::LPAREN);
      setState(158);
      expr();
      setState(159);
      match(RccIRParser::RPAREN);
      setState(160);
      stmt();
      setState(163);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 20, _ctx)) {
      case 1: {
        setState(161);
        match(RccIRParser::POST_KW);
        setState(162);
        stmt();
        break;
      }

      default:
        break;
      }
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<RccIRParser::ReturnStmtContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(165);
      match(RccIRParser::RETURN_KW);
      setState(167);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & -4611685985139818496) != 0)) {
        setState(166);
        expr();
      }
      setState(169);
      match(RccIRParser::SEMI);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<RccIRParser::BreakStmtContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(170);
      match(RccIRParser::BREAK_KW);
      setState(171);
      match(RccIRParser::SEMI);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<RccIRParser::ContinueStmtContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(172);
      match(RccIRParser::CONTINUE_KW);
      setState(173);
      match(RccIRParser::SEMI);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<RccIRParser::LocalDeclStmtContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(174);
      match(RccIRParser::LOCAL_KW);
      setState(175);
      typeSpec();
      setState(176);
      ref();
      setState(188);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case RccIRParser::LBRACK: {
          setState(177);
          match(RccIRParser::LBRACK);
          setState(178);
          match(RccIRParser::NUMBER);
          setState(179);
          match(RccIRParser::RBRACK);
          setState(182);
          _errHandler->sync(this);

          _la = _input->LA(1);
          if (_la == RccIRParser::ASSIGN) {
            setState(180);
            match(RccIRParser::ASSIGN);
            setState(181);
            initList();
          }
          break;
        }

        case RccIRParser::SEMI:
        case RccIRParser::ASSIGN: {
          setState(186);
          _errHandler->sync(this);

          _la = _input->LA(1);
          if (_la == RccIRParser::ASSIGN) {
            setState(184);
            match(RccIRParser::ASSIGN);
            setState(185);
            expr();
          }
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(190);
      match(RccIRParser::SEMI);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<RccIRParser::AsmStmtContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(192);
      match(RccIRParser::ASM_BEGIN);
      setState(193);
      match(RccIRParser::ASM_TEXT);
      setState(194);
      match(RccIRParser::ASM_END);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<RccIRParser::EmptyStmtContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(195);
      match(RccIRParser::SEMI);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<RccIRParser::LabelStmtContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(196);
      match(RccIRParser::IDENT);
      setState(197);
      match(RccIRParser::COLON);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<RccIRParser::GotoStmtContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(198);
      match(RccIRParser::GOTO_KW);
      setState(199);
      match(RccIRParser::IDENT);
      setState(200);
      match(RccIRParser::SEMI);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<RccIRParser::ExprStmtContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(201);
      expr();
      setState(202);
      match(RccIRParser::SEMI);
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

//----------------- RefContext ------------------------------------------------------------------

RccIRParser::RefContext::RefContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccIRParser::RefContext::IDENT() {
  return getToken(RccIRParser::IDENT, 0);
}

tree::TerminalNode* RccIRParser::RefContext::HASH() {
  return getToken(RccIRParser::HASH, 0);
}

tree::TerminalNode* RccIRParser::RefContext::NUMBER() {
  return getToken(RccIRParser::NUMBER, 0);
}


size_t RccIRParser::RefContext::getRuleIndex() const {
  return RccIRParser::RuleRef;
}


std::any RccIRParser::RefContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitRef(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::RefContext* RccIRParser::ref() {
  RefContext *_localctx = _tracker.createInstance<RefContext>(_ctx, getState());
  enterRule(_localctx, 22, RccIRParser::RuleRef);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(206);
    match(RccIRParser::IDENT);
    setState(207);
    match(RccIRParser::HASH);
    setState(208);
    match(RccIRParser::NUMBER);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CompoundOpContext ------------------------------------------------------------------

RccIRParser::CompoundOpContext::CompoundOpContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccIRParser::CompoundOpContext::PLUSEQ() {
  return getToken(RccIRParser::PLUSEQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::MINUSEQ() {
  return getToken(RccIRParser::MINUSEQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::STAREQ() {
  return getToken(RccIRParser::STAREQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::SLASHEQ() {
  return getToken(RccIRParser::SLASHEQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::PERCENTEQ() {
  return getToken(RccIRParser::PERCENTEQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::AMPEQ() {
  return getToken(RccIRParser::AMPEQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::CARETEQ() {
  return getToken(RccIRParser::CARETEQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::PIPEEQ() {
  return getToken(RccIRParser::PIPEEQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::SHLEQ() {
  return getToken(RccIRParser::SHLEQ, 0);
}

tree::TerminalNode* RccIRParser::CompoundOpContext::SHREQ() {
  return getToken(RccIRParser::SHREQ, 0);
}


size_t RccIRParser::CompoundOpContext::getRuleIndex() const {
  return RccIRParser::RuleCompoundOp;
}


std::any RccIRParser::CompoundOpContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitCompoundOp(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::CompoundOpContext* RccIRParser::compoundOp() {
  CompoundOpContext *_localctx = _tracker.createInstance<CompoundOpContext>(_ctx, getState());
  enterRule(_localctx, 24, RccIRParser::RuleCompoundOp);
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
    setState(210);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1151795604700004352) != 0))) {
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

RccIRParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t RccIRParser::ExprContext::getRuleIndex() const {
  return RccIRParser::RuleExpr;
}

void RccIRParser::ExprContext::copyFrom(ExprContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- CompoundDerefExprContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::CompoundDerefExprContext::STAR() {
  return getToken(RccIRParser::STAR, 0);
}

tree::TerminalNode* RccIRParser::CompoundDerefExprContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

std::vector<RccIRParser::ExprContext *> RccIRParser::CompoundDerefExprContext::expr() {
  return getRuleContexts<RccIRParser::ExprContext>();
}

RccIRParser::ExprContext* RccIRParser::CompoundDerefExprContext::expr(size_t i) {
  return getRuleContext<RccIRParser::ExprContext>(i);
}

tree::TerminalNode* RccIRParser::CompoundDerefExprContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

RccIRParser::CompoundOpContext* RccIRParser::CompoundDerefExprContext::compoundOp() {
  return getRuleContext<RccIRParser::CompoundOpContext>(0);
}

tree::TerminalNode* RccIRParser::CompoundDerefExprContext::OLDVAL_KW() {
  return getToken(RccIRParser::OLDVAL_KW, 0);
}

RccIRParser::CompoundDerefExprContext::CompoundDerefExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::CompoundDerefExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitCompoundDerefExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- StarExprContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::StarExprContext::STAR() {
  return getToken(RccIRParser::STAR, 0);
}

tree::TerminalNode* RccIRParser::StarExprContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

std::vector<RccIRParser::ExprContext *> RccIRParser::StarExprContext::expr() {
  return getRuleContexts<RccIRParser::ExprContext>();
}

RccIRParser::ExprContext* RccIRParser::StarExprContext::expr(size_t i) {
  return getRuleContext<RccIRParser::ExprContext>(i);
}

tree::TerminalNode* RccIRParser::StarExprContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

tree::TerminalNode* RccIRParser::StarExprContext::ASSIGN() {
  return getToken(RccIRParser::ASSIGN, 0);
}

RccIRParser::StarExprContext::StarExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::StarExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitStarExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CompoundAssignExprContext ------------------------------------------------------------------

RccIRParser::RefContext* RccIRParser::CompoundAssignExprContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

RccIRParser::CompoundOpContext* RccIRParser::CompoundAssignExprContext::compoundOp() {
  return getRuleContext<RccIRParser::CompoundOpContext>(0);
}

RccIRParser::ExprContext* RccIRParser::CompoundAssignExprContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

tree::TerminalNode* RccIRParser::CompoundAssignExprContext::OLDVAL_KW() {
  return getToken(RccIRParser::OLDVAL_KW, 0);
}

RccIRParser::CompoundAssignExprContext::CompoundAssignExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::CompoundAssignExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitCompoundAssignExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IntLitExprContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::IntLitExprContext::NUMBER() {
  return getToken(RccIRParser::NUMBER, 0);
}

RccIRParser::IntLitExprContext::IntLitExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::IntLitExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitIntLitExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BinaryExprContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::BinaryExprContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

std::vector<RccIRParser::ExprContext *> RccIRParser::BinaryExprContext::expr() {
  return getRuleContexts<RccIRParser::ExprContext>();
}

RccIRParser::ExprContext* RccIRParser::BinaryExprContext::expr(size_t i) {
  return getRuleContext<RccIRParser::ExprContext>(i);
}

RccIRParser::BinOpContext* RccIRParser::BinaryExprContext::binOp() {
  return getRuleContext<RccIRParser::BinOpContext>(0);
}

tree::TerminalNode* RccIRParser::BinaryExprContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

RccIRParser::BinaryExprContext::BinaryExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::BinaryExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitBinaryExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CallPtrExprContext ------------------------------------------------------------------

std::vector<tree::TerminalNode *> RccIRParser::CallPtrExprContext::LPAREN() {
  return getTokens(RccIRParser::LPAREN);
}

tree::TerminalNode* RccIRParser::CallPtrExprContext::LPAREN(size_t i) {
  return getToken(RccIRParser::LPAREN, i);
}

RccIRParser::ExprContext* RccIRParser::CallPtrExprContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

std::vector<tree::TerminalNode *> RccIRParser::CallPtrExprContext::RPAREN() {
  return getTokens(RccIRParser::RPAREN);
}

tree::TerminalNode* RccIRParser::CallPtrExprContext::RPAREN(size_t i) {
  return getToken(RccIRParser::RPAREN, i);
}

RccIRParser::CallPtrExprContext::CallPtrExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::CallPtrExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitCallPtrExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IndexExprContext ------------------------------------------------------------------

RccIRParser::RefContext* RccIRParser::IndexExprContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

tree::TerminalNode* RccIRParser::IndexExprContext::LBRACK() {
  return getToken(RccIRParser::LBRACK, 0);
}

std::vector<RccIRParser::ExprContext *> RccIRParser::IndexExprContext::expr() {
  return getRuleContexts<RccIRParser::ExprContext>();
}

RccIRParser::ExprContext* RccIRParser::IndexExprContext::expr(size_t i) {
  return getRuleContext<RccIRParser::ExprContext>(i);
}

tree::TerminalNode* RccIRParser::IndexExprContext::RBRACK() {
  return getToken(RccIRParser::RBRACK, 0);
}

tree::TerminalNode* RccIRParser::IndexExprContext::ASSIGN() {
  return getToken(RccIRParser::ASSIGN, 0);
}

RccIRParser::IndexExprContext::IndexExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::IndexExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitIndexExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- VarExprContext ------------------------------------------------------------------

RccIRParser::RefContext* RccIRParser::VarExprContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

RccIRParser::VarExprContext::VarExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::VarExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitVarExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IntrinsicExprContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::IntrinsicExprContext::IDENT() {
  return getToken(RccIRParser::IDENT, 0);
}

tree::TerminalNode* RccIRParser::IntrinsicExprContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

tree::TerminalNode* RccIRParser::IntrinsicExprContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

RccIRParser::IntrinsicExprContext::IntrinsicExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::IntrinsicExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitIntrinsicExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- UnaryExprContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::UnaryExprContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

RccIRParser::ExprContext* RccIRParser::UnaryExprContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

tree::TerminalNode* RccIRParser::UnaryExprContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

tree::TerminalNode* RccIRParser::UnaryExprContext::MINUS() {
  return getToken(RccIRParser::MINUS, 0);
}

tree::TerminalNode* RccIRParser::UnaryExprContext::BANG() {
  return getToken(RccIRParser::BANG, 0);
}

tree::TerminalNode* RccIRParser::UnaryExprContext::TILDE() {
  return getToken(RccIRParser::TILDE, 0);
}

tree::TerminalNode* RccIRParser::UnaryExprContext::AMP() {
  return getToken(RccIRParser::AMP, 0);
}

RccIRParser::UnaryExprContext::UnaryExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::UnaryExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitUnaryExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- TernaryExprContext ------------------------------------------------------------------

tree::TerminalNode* RccIRParser::TernaryExprContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

std::vector<RccIRParser::ExprContext *> RccIRParser::TernaryExprContext::expr() {
  return getRuleContexts<RccIRParser::ExprContext>();
}

RccIRParser::ExprContext* RccIRParser::TernaryExprContext::expr(size_t i) {
  return getRuleContext<RccIRParser::ExprContext>(i);
}

tree::TerminalNode* RccIRParser::TernaryExprContext::QUESTION() {
  return getToken(RccIRParser::QUESTION, 0);
}

tree::TerminalNode* RccIRParser::TernaryExprContext::COLON() {
  return getToken(RccIRParser::COLON, 0);
}

tree::TerminalNode* RccIRParser::TernaryExprContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

RccIRParser::TernaryExprContext::TernaryExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::TernaryExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitTernaryExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ConvertExprContext ------------------------------------------------------------------

std::vector<tree::TerminalNode *> RccIRParser::ConvertExprContext::LPAREN() {
  return getTokens(RccIRParser::LPAREN);
}

tree::TerminalNode* RccIRParser::ConvertExprContext::LPAREN(size_t i) {
  return getToken(RccIRParser::LPAREN, i);
}

RccIRParser::TypeSpecContext* RccIRParser::ConvertExprContext::typeSpec() {
  return getRuleContext<RccIRParser::TypeSpecContext>(0);
}

std::vector<tree::TerminalNode *> RccIRParser::ConvertExprContext::RPAREN() {
  return getTokens(RccIRParser::RPAREN);
}

tree::TerminalNode* RccIRParser::ConvertExprContext::RPAREN(size_t i) {
  return getToken(RccIRParser::RPAREN, i);
}

RccIRParser::ExprContext* RccIRParser::ConvertExprContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

RccIRParser::ConvertExprContext::ConvertExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::ConvertExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitConvertExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CallExprContext ------------------------------------------------------------------

RccIRParser::RefContext* RccIRParser::CallExprContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

tree::TerminalNode* RccIRParser::CallExprContext::LPAREN() {
  return getToken(RccIRParser::LPAREN, 0);
}

tree::TerminalNode* RccIRParser::CallExprContext::RPAREN() {
  return getToken(RccIRParser::RPAREN, 0);
}

RccIRParser::ArgListContext* RccIRParser::CallExprContext::argList() {
  return getRuleContext<RccIRParser::ArgListContext>(0);
}

RccIRParser::CallExprContext::CallExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::CallExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitCallExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AssignExprContext ------------------------------------------------------------------

RccIRParser::RefContext* RccIRParser::AssignExprContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

tree::TerminalNode* RccIRParser::AssignExprContext::ASSIGN() {
  return getToken(RccIRParser::ASSIGN, 0);
}

RccIRParser::ExprContext* RccIRParser::AssignExprContext::expr() {
  return getRuleContext<RccIRParser::ExprContext>(0);
}

RccIRParser::AssignExprContext::AssignExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::AssignExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitAssignExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CompoundIndexExprContext ------------------------------------------------------------------

RccIRParser::RefContext* RccIRParser::CompoundIndexExprContext::ref() {
  return getRuleContext<RccIRParser::RefContext>(0);
}

tree::TerminalNode* RccIRParser::CompoundIndexExprContext::LBRACK() {
  return getToken(RccIRParser::LBRACK, 0);
}

std::vector<RccIRParser::ExprContext *> RccIRParser::CompoundIndexExprContext::expr() {
  return getRuleContexts<RccIRParser::ExprContext>();
}

RccIRParser::ExprContext* RccIRParser::CompoundIndexExprContext::expr(size_t i) {
  return getRuleContext<RccIRParser::ExprContext>(i);
}

tree::TerminalNode* RccIRParser::CompoundIndexExprContext::RBRACK() {
  return getToken(RccIRParser::RBRACK, 0);
}

RccIRParser::CompoundOpContext* RccIRParser::CompoundIndexExprContext::compoundOp() {
  return getRuleContext<RccIRParser::CompoundOpContext>(0);
}

tree::TerminalNode* RccIRParser::CompoundIndexExprContext::OLDVAL_KW() {
  return getToken(RccIRParser::OLDVAL_KW, 0);
}

RccIRParser::CompoundIndexExprContext::CompoundIndexExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any RccIRParser::CompoundIndexExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitCompoundIndexExpr(this);
  else
    return visitor->visitChildren(this);
}
RccIRParser::ExprContext* RccIRParser::expr() {
  ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, getState());
  enterRule(_localctx, 26, RccIRParser::RuleExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(303);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 32, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<RccIRParser::IntLitExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(212);
      match(RccIRParser::NUMBER);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<RccIRParser::CompoundIndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(214);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::OLDVAL_KW) {
        setState(213);
        match(RccIRParser::OLDVAL_KW);
      }
      setState(216);
      ref();
      setState(217);
      match(RccIRParser::LBRACK);
      setState(218);
      expr();
      setState(219);
      match(RccIRParser::RBRACK);
      setState(220);
      compoundOp();
      setState(221);
      expr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<RccIRParser::CompoundAssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(224);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::OLDVAL_KW) {
        setState(223);
        match(RccIRParser::OLDVAL_KW);
      }
      setState(226);
      ref();
      setState(227);
      compoundOp();
      setState(228);
      expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<RccIRParser::CompoundDerefExprContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(231);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::OLDVAL_KW) {
        setState(230);
        match(RccIRParser::OLDVAL_KW);
      }
      setState(233);
      match(RccIRParser::STAR);
      setState(234);
      match(RccIRParser::LPAREN);
      setState(235);
      expr();
      setState(236);
      match(RccIRParser::RPAREN);
      setState(237);
      compoundOp();
      setState(238);
      expr();
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<RccIRParser::IndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(240);
      ref();
      setState(241);
      match(RccIRParser::LBRACK);
      setState(242);
      expr();
      setState(243);
      match(RccIRParser::RBRACK);
      setState(246);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::ASSIGN) {
        setState(244);
        match(RccIRParser::ASSIGN);
        setState(245);
        expr();
      }
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<RccIRParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(248);
      ref();
      setState(249);
      match(RccIRParser::ASSIGN);
      setState(250);
      expr();
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<RccIRParser::StarExprContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(252);
      match(RccIRParser::STAR);
      setState(253);
      match(RccIRParser::LPAREN);
      setState(254);
      expr();
      setState(255);
      match(RccIRParser::RPAREN);
      setState(258);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::ASSIGN) {
        setState(256);
        match(RccIRParser::ASSIGN);
        setState(257);
        expr();
      }
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<RccIRParser::UnaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(260);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 32212254720) != 0))) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(261);
      match(RccIRParser::LPAREN);
      setState(262);
      expr();
      setState(263);
      match(RccIRParser::RPAREN);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<RccIRParser::BinaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(265);
      match(RccIRParser::LPAREN);
      setState(266);
      expr();
      setState(267);
      binOp();
      setState(268);
      expr();
      setState(269);
      match(RccIRParser::RPAREN);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<RccIRParser::TernaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(271);
      match(RccIRParser::LPAREN);
      setState(272);
      expr();
      setState(273);
      match(RccIRParser::QUESTION);
      setState(274);
      expr();
      setState(275);
      match(RccIRParser::COLON);
      setState(276);
      expr();
      setState(277);
      match(RccIRParser::RPAREN);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<RccIRParser::ConvertExprContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(279);
      match(RccIRParser::LPAREN);
      setState(280);
      typeSpec();
      setState(281);
      match(RccIRParser::RPAREN);
      setState(282);
      match(RccIRParser::LPAREN);
      setState(283);
      expr();
      setState(284);
      match(RccIRParser::RPAREN);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<RccIRParser::CallExprContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(286);
      ref();
      setState(287);
      match(RccIRParser::LPAREN);
      setState(289);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & -4611685985139818496) != 0)) {
        setState(288);
        argList();
      }
      setState(291);
      match(RccIRParser::RPAREN);
      break;
    }

    case 13: {
      _localctx = _tracker.createInstance<RccIRParser::CallPtrExprContext>(_localctx);
      enterOuterAlt(_localctx, 13);
      setState(293);
      match(RccIRParser::LPAREN);
      setState(294);
      expr();
      setState(295);
      match(RccIRParser::RPAREN);
      setState(296);
      match(RccIRParser::LPAREN);
      setState(297);
      match(RccIRParser::RPAREN);
      break;
    }

    case 14: {
      _localctx = _tracker.createInstance<RccIRParser::IntrinsicExprContext>(_localctx);
      enterOuterAlt(_localctx, 14);
      setState(299);
      match(RccIRParser::IDENT);
      setState(300);
      match(RccIRParser::LPAREN);
      setState(301);
      match(RccIRParser::RPAREN);
      break;
    }

    case 15: {
      _localctx = _tracker.createInstance<RccIRParser::VarExprContext>(_localctx);
      enterOuterAlt(_localctx, 15);
      setState(302);
      ref();
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

//----------------- ArgListContext ------------------------------------------------------------------

RccIRParser::ArgListContext::ArgListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<RccIRParser::ExprContext *> RccIRParser::ArgListContext::expr() {
  return getRuleContexts<RccIRParser::ExprContext>();
}

RccIRParser::ExprContext* RccIRParser::ArgListContext::expr(size_t i) {
  return getRuleContext<RccIRParser::ExprContext>(i);
}

std::vector<tree::TerminalNode *> RccIRParser::ArgListContext::COMMA() {
  return getTokens(RccIRParser::COMMA);
}

tree::TerminalNode* RccIRParser::ArgListContext::COMMA(size_t i) {
  return getToken(RccIRParser::COMMA, i);
}


size_t RccIRParser::ArgListContext::getRuleIndex() const {
  return RccIRParser::RuleArgList;
}


std::any RccIRParser::ArgListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitArgList(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::ArgListContext* RccIRParser::argList() {
  ArgListContext *_localctx = _tracker.createInstance<ArgListContext>(_ctx, getState());
  enterRule(_localctx, 28, RccIRParser::RuleArgList);
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
    setState(305);
    expr();
    setState(310);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == RccIRParser::COMMA) {
      setState(306);
      match(RccIRParser::COMMA);
      setState(307);
      expr();
      setState(312);
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

//----------------- BinOpContext ------------------------------------------------------------------

RccIRParser::BinOpContext::BinOpContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccIRParser::BinOpContext::PLUS() {
  return getToken(RccIRParser::PLUS, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::MINUS() {
  return getToken(RccIRParser::MINUS, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::STAR() {
  return getToken(RccIRParser::STAR, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::SLASH() {
  return getToken(RccIRParser::SLASH, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::PERCENT() {
  return getToken(RccIRParser::PERCENT, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::SHL() {
  return getToken(RccIRParser::SHL, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::SHR() {
  return getToken(RccIRParser::SHR, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::EQ() {
  return getToken(RccIRParser::EQ, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::NE() {
  return getToken(RccIRParser::NE, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::LT() {
  return getToken(RccIRParser::LT, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::LE() {
  return getToken(RccIRParser::LE, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::GT() {
  return getToken(RccIRParser::GT, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::GE() {
  return getToken(RccIRParser::GE, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::AMP() {
  return getToken(RccIRParser::AMP, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::CARET() {
  return getToken(RccIRParser::CARET, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::PIPE() {
  return getToken(RccIRParser::PIPE, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::ANDAND() {
  return getToken(RccIRParser::ANDAND, 0);
}

tree::TerminalNode* RccIRParser::BinOpContext::OROR() {
  return getToken(RccIRParser::OROR, 0);
}


size_t RccIRParser::BinOpContext::getRuleIndex() const {
  return RccIRParser::RuleBinOp;
}


std::any RccIRParser::BinOpContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccIRParserVisitor*>(visitor))
    return parserVisitor->visitBinOp(this);
  else
    return visitor->visitChildren(this);
}

RccIRParser::BinOpContext* RccIRParser::binOp() {
  BinOpContext *_localctx = _tracker.createInstance<BinOpContext>(_ctx, getState());
  enterRule(_localctx, 30, RccIRParser::RuleBinOp);
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
    setState(313);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1125873063297024) != 0))) {
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

void RccIRParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  rccirparserParserInitialize();
#else
  ::antlr4::internal::call_once(rccirparserParserOnceFlag, rccirparserParserInitialize);
#endif
}
