
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
      "'short'", "'long'", "'unsigned'", "'void'", "'local'", "'if'", "'else'", 
      "'while'", "'return'", "'break'", "'continue'", "'goto'", "'post'", 
      "'oldval'", "'('", "')'", "'['", "']'", "'{'", "", "';'", "','", "'='", 
      "'#'", "'*'", "'&'", "'-'", "'!'", "'~'", "'/'", "'%'", "'+'", "'<<'", 
      "'>>'", "'<='", "'>='", "'<'", "'>'", "'=='", "'!='", "'^'", "'|'", 
      "'&&'", "'||'", "'+='", "'-='", "'*='", "'/='", "'%='", "'&='", "'^='", 
      "'|='", "'<<='", "'>>='", "'\\u003F'", "':'"
    },
    std::vector<std::string>{
      "", "GLOBALS_HEADER", "FUNCTIONS_HEADER", "INT_KW", "CHAR_KW", "SHORT_KW", 
      "LONG_KW", "UNSIGNED_KW", "VOID_KW", "LOCAL_KW", "IF_KW", "ELSE_KW", 
      "WHILE_KW", "RETURN_KW", "BREAK_KW", "CONTINUE_KW", "GOTO_KW", "POST_KW", 
      "OLDVAL_KW", "LPAREN", "RPAREN", "LBRACK", "RBRACK", "LBRACE", "RBRACE", 
      "SEMI", "COMMA", "ASSIGN", "HASH", "STAR", "AMP", "MINUS", "BANG", 
      "TILDE", "SLASH", "PERCENT", "PLUS", "SHL", "SHR", "LE", "GE", "LT", 
      "GT", "EQ", "NE", "CARET", "PIPE", "ANDAND", "OROR", "PLUSEQ", "MINUSEQ", 
      "STAREQ", "SLASHEQ", "PERCENTEQ", "AMPEQ", "CARETEQ", "PIPEEQ", "SHLEQ", 
      "SHREQ", "QUESTION", "COLON", "IDENT", "NUMBER", "WS", "ASM_BEGIN", 
      "ASM_TEXT", "ASM_END"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,66,314,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,1,0,1,0,5,0,35,8,0,10,0,12,0,38,9,0,1,0,1,0,5,0,42,8,0,10,
  	0,12,0,45,9,0,1,0,1,0,1,1,3,1,50,8,1,1,1,1,1,3,1,54,8,1,1,1,1,1,3,1,58,
  	8,1,1,1,1,1,3,1,62,8,1,1,1,1,1,1,1,3,1,67,8,1,1,1,3,1,70,8,1,1,1,5,1,
  	73,8,1,10,1,12,1,76,9,1,1,2,1,2,1,2,1,2,1,2,1,2,1,2,3,2,85,8,2,1,2,1,
  	2,3,2,89,8,2,3,2,91,8,2,1,2,1,2,1,3,1,3,1,3,1,3,5,3,99,8,3,10,3,12,3,
  	102,9,3,3,3,104,8,3,1,3,1,3,1,4,1,4,3,4,110,8,4,1,5,1,5,1,5,1,5,3,5,116,
  	8,5,1,5,1,5,1,5,1,6,1,6,3,6,123,8,6,1,7,1,7,1,7,5,7,128,8,7,10,7,12,7,
  	131,9,7,1,8,1,8,1,8,1,9,1,9,5,9,138,8,9,10,9,12,9,141,9,9,1,9,1,9,1,10,
  	1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,153,8,10,1,10,1,10,1,10,1,10,
  	1,10,1,10,1,10,3,10,162,8,10,1,10,1,10,3,10,166,8,10,1,10,1,10,1,10,1,
  	10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,181,8,10,1,10,1,
  	10,3,10,185,8,10,3,10,187,8,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,
  	1,10,1,10,1,10,1,10,1,10,1,10,3,10,203,8,10,1,11,1,11,1,11,1,11,1,12,
  	1,12,1,13,1,13,3,13,213,8,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,
  	3,13,223,8,13,1,13,1,13,1,13,1,13,1,13,3,13,230,8,13,1,13,1,13,1,13,1,
  	13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,245,8,13,1,13,1,
  	13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,257,8,13,1,13,1,13,1,
  	13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,
  	13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,288,
  	8,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,
  	302,8,13,1,14,1,14,1,14,5,14,307,8,14,10,14,12,14,310,9,14,1,15,1,15,
  	1,15,0,0,16,0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,0,3,1,0,49,58,
  	1,0,30,33,2,0,29,31,34,48,357,0,32,1,0,0,0,2,69,1,0,0,0,4,77,1,0,0,0,
  	6,94,1,0,0,0,8,109,1,0,0,0,10,111,1,0,0,0,12,122,1,0,0,0,14,124,1,0,0,
  	0,16,132,1,0,0,0,18,135,1,0,0,0,20,202,1,0,0,0,22,204,1,0,0,0,24,208,
  	1,0,0,0,26,301,1,0,0,0,28,303,1,0,0,0,30,311,1,0,0,0,32,36,5,1,0,0,33,
  	35,3,4,2,0,34,33,1,0,0,0,35,38,1,0,0,0,36,34,1,0,0,0,36,37,1,0,0,0,37,
  	39,1,0,0,0,38,36,1,0,0,0,39,43,5,2,0,0,40,42,3,10,5,0,41,40,1,0,0,0,42,
  	45,1,0,0,0,43,41,1,0,0,0,43,44,1,0,0,0,44,46,1,0,0,0,45,43,1,0,0,0,46,
  	47,5,0,0,1,47,1,1,0,0,0,48,50,5,7,0,0,49,48,1,0,0,0,49,50,1,0,0,0,50,
  	51,1,0,0,0,51,70,5,4,0,0,52,54,5,7,0,0,53,52,1,0,0,0,53,54,1,0,0,0,54,
  	55,1,0,0,0,55,70,5,5,0,0,56,58,5,7,0,0,57,56,1,0,0,0,57,58,1,0,0,0,58,
  	59,1,0,0,0,59,70,5,3,0,0,60,62,5,7,0,0,61,60,1,0,0,0,61,62,1,0,0,0,62,
  	63,1,0,0,0,63,64,5,6,0,0,64,70,5,6,0,0,65,67,5,7,0,0,66,65,1,0,0,0,66,
  	67,1,0,0,0,67,68,1,0,0,0,68,70,5,6,0,0,69,49,1,0,0,0,69,53,1,0,0,0,69,
  	57,1,0,0,0,69,61,1,0,0,0,69,66,1,0,0,0,70,74,1,0,0,0,71,73,5,29,0,0,72,
  	71,1,0,0,0,73,76,1,0,0,0,74,72,1,0,0,0,74,75,1,0,0,0,75,3,1,0,0,0,76,
  	74,1,0,0,0,77,78,3,2,1,0,78,90,3,22,11,0,79,80,5,21,0,0,80,81,5,62,0,
  	0,81,84,5,22,0,0,82,83,5,27,0,0,83,85,3,6,3,0,84,82,1,0,0,0,84,85,1,0,
  	0,0,85,91,1,0,0,0,86,87,5,27,0,0,87,89,5,62,0,0,88,86,1,0,0,0,88,89,1,
  	0,0,0,89,91,1,0,0,0,90,79,1,0,0,0,90,88,1,0,0,0,91,92,1,0,0,0,92,93,5,
  	25,0,0,93,5,1,0,0,0,94,103,5,23,0,0,95,100,3,8,4,0,96,97,5,26,0,0,97,
  	99,3,8,4,0,98,96,1,0,0,0,99,102,1,0,0,0,100,98,1,0,0,0,100,101,1,0,0,
  	0,101,104,1,0,0,0,102,100,1,0,0,0,103,95,1,0,0,0,103,104,1,0,0,0,104,
  	105,1,0,0,0,105,106,5,24,0,0,106,7,1,0,0,0,107,110,5,62,0,0,108,110,3,
  	22,11,0,109,107,1,0,0,0,109,108,1,0,0,0,110,9,1,0,0,0,111,112,3,12,6,
  	0,112,113,3,22,11,0,113,115,5,19,0,0,114,116,3,14,7,0,115,114,1,0,0,0,
  	115,116,1,0,0,0,116,117,1,0,0,0,117,118,5,20,0,0,118,119,3,18,9,0,119,
  	11,1,0,0,0,120,123,3,2,1,0,121,123,5,8,0,0,122,120,1,0,0,0,122,121,1,
  	0,0,0,123,13,1,0,0,0,124,129,3,16,8,0,125,126,5,26,0,0,126,128,3,16,8,
  	0,127,125,1,0,0,0,128,131,1,0,0,0,129,127,1,0,0,0,129,130,1,0,0,0,130,
  	15,1,0,0,0,131,129,1,0,0,0,132,133,3,2,1,0,133,134,3,22,11,0,134,17,1,
  	0,0,0,135,139,5,23,0,0,136,138,3,20,10,0,137,136,1,0,0,0,138,141,1,0,
  	0,0,139,137,1,0,0,0,139,140,1,0,0,0,140,142,1,0,0,0,141,139,1,0,0,0,142,
  	143,5,24,0,0,143,19,1,0,0,0,144,203,3,18,9,0,145,146,5,10,0,0,146,147,
  	5,19,0,0,147,148,3,26,13,0,148,149,5,20,0,0,149,152,3,20,10,0,150,151,
  	5,11,0,0,151,153,3,20,10,0,152,150,1,0,0,0,152,153,1,0,0,0,153,203,1,
  	0,0,0,154,155,5,12,0,0,155,156,5,19,0,0,156,157,3,26,13,0,157,158,5,20,
  	0,0,158,161,3,20,10,0,159,160,5,17,0,0,160,162,3,20,10,0,161,159,1,0,
  	0,0,161,162,1,0,0,0,162,203,1,0,0,0,163,165,5,13,0,0,164,166,3,26,13,
  	0,165,164,1,0,0,0,165,166,1,0,0,0,166,167,1,0,0,0,167,203,5,25,0,0,168,
  	169,5,14,0,0,169,203,5,25,0,0,170,171,5,15,0,0,171,203,5,25,0,0,172,173,
  	5,9,0,0,173,174,3,2,1,0,174,186,3,22,11,0,175,176,5,21,0,0,176,177,5,
  	62,0,0,177,180,5,22,0,0,178,179,5,27,0,0,179,181,3,6,3,0,180,178,1,0,
  	0,0,180,181,1,0,0,0,181,187,1,0,0,0,182,183,5,27,0,0,183,185,3,26,13,
  	0,184,182,1,0,0,0,184,185,1,0,0,0,185,187,1,0,0,0,186,175,1,0,0,0,186,
  	184,1,0,0,0,187,188,1,0,0,0,188,189,5,25,0,0,189,203,1,0,0,0,190,191,
  	5,64,0,0,191,192,5,65,0,0,192,203,5,66,0,0,193,203,5,25,0,0,194,195,5,
  	61,0,0,195,203,5,60,0,0,196,197,5,16,0,0,197,198,5,61,0,0,198,203,5,25,
  	0,0,199,200,3,26,13,0,200,201,5,25,0,0,201,203,1,0,0,0,202,144,1,0,0,
  	0,202,145,1,0,0,0,202,154,1,0,0,0,202,163,1,0,0,0,202,168,1,0,0,0,202,
  	170,1,0,0,0,202,172,1,0,0,0,202,190,1,0,0,0,202,193,1,0,0,0,202,194,1,
  	0,0,0,202,196,1,0,0,0,202,199,1,0,0,0,203,21,1,0,0,0,204,205,5,61,0,0,
  	205,206,5,28,0,0,206,207,5,62,0,0,207,23,1,0,0,0,208,209,7,0,0,0,209,
  	25,1,0,0,0,210,302,5,62,0,0,211,213,5,18,0,0,212,211,1,0,0,0,212,213,
  	1,0,0,0,213,214,1,0,0,0,214,215,3,22,11,0,215,216,5,21,0,0,216,217,3,
  	26,13,0,217,218,5,22,0,0,218,219,3,24,12,0,219,220,3,26,13,0,220,302,
  	1,0,0,0,221,223,5,18,0,0,222,221,1,0,0,0,222,223,1,0,0,0,223,224,1,0,
  	0,0,224,225,3,22,11,0,225,226,3,24,12,0,226,227,3,26,13,0,227,302,1,0,
  	0,0,228,230,5,18,0,0,229,228,1,0,0,0,229,230,1,0,0,0,230,231,1,0,0,0,
  	231,232,5,29,0,0,232,233,5,19,0,0,233,234,3,26,13,0,234,235,5,20,0,0,
  	235,236,3,24,12,0,236,237,3,26,13,0,237,302,1,0,0,0,238,239,3,22,11,0,
  	239,240,5,21,0,0,240,241,3,26,13,0,241,244,5,22,0,0,242,243,5,27,0,0,
  	243,245,3,26,13,0,244,242,1,0,0,0,244,245,1,0,0,0,245,302,1,0,0,0,246,
  	247,3,22,11,0,247,248,5,27,0,0,248,249,3,26,13,0,249,302,1,0,0,0,250,
  	251,5,29,0,0,251,252,5,19,0,0,252,253,3,26,13,0,253,256,5,20,0,0,254,
  	255,5,27,0,0,255,257,3,26,13,0,256,254,1,0,0,0,256,257,1,0,0,0,257,302,
  	1,0,0,0,258,259,7,1,0,0,259,260,5,19,0,0,260,261,3,26,13,0,261,262,5,
  	20,0,0,262,302,1,0,0,0,263,264,5,19,0,0,264,265,3,26,13,0,265,266,3,30,
  	15,0,266,267,3,26,13,0,267,268,5,20,0,0,268,302,1,0,0,0,269,270,5,19,
  	0,0,270,271,3,26,13,0,271,272,5,59,0,0,272,273,3,26,13,0,273,274,5,60,
  	0,0,274,275,3,26,13,0,275,276,5,20,0,0,276,302,1,0,0,0,277,278,5,19,0,
  	0,278,279,3,2,1,0,279,280,5,20,0,0,280,281,5,19,0,0,281,282,3,26,13,0,
  	282,283,5,20,0,0,283,302,1,0,0,0,284,285,3,22,11,0,285,287,5,19,0,0,286,
  	288,3,28,14,0,287,286,1,0,0,0,287,288,1,0,0,0,288,289,1,0,0,0,289,290,
  	5,20,0,0,290,302,1,0,0,0,291,292,5,19,0,0,292,293,3,26,13,0,293,294,5,
  	20,0,0,294,295,5,19,0,0,295,296,5,20,0,0,296,302,1,0,0,0,297,298,5,61,
  	0,0,298,299,5,19,0,0,299,302,5,20,0,0,300,302,3,22,11,0,301,210,1,0,0,
  	0,301,212,1,0,0,0,301,222,1,0,0,0,301,229,1,0,0,0,301,238,1,0,0,0,301,
  	246,1,0,0,0,301,250,1,0,0,0,301,258,1,0,0,0,301,263,1,0,0,0,301,269,1,
  	0,0,0,301,277,1,0,0,0,301,284,1,0,0,0,301,291,1,0,0,0,301,297,1,0,0,0,
  	301,300,1,0,0,0,302,27,1,0,0,0,303,308,3,26,13,0,304,305,5,26,0,0,305,
  	307,3,26,13,0,306,304,1,0,0,0,307,310,1,0,0,0,308,306,1,0,0,0,308,309,
  	1,0,0,0,309,29,1,0,0,0,310,308,1,0,0,0,311,312,7,2,0,0,312,31,1,0,0,0,
  	34,36,43,49,53,57,61,66,69,74,84,88,90,100,103,109,115,122,129,139,152,
  	161,165,180,184,186,202,212,222,229,244,256,287,301,308
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
      ((1ULL << _la) & 248) != 0)) {
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
      ((1ULL << _la) & 504) != 0)) {
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
    setState(69);
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

    default:
      break;
    }
    setState(74);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == RccIRParser::STAR) {
      setState(71);
      match(RccIRParser::STAR);
      setState(76);
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
    setState(77);
    typeSpec();
    setState(78);
    ref();
    setState(90);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case RccIRParser::LBRACK: {
        setState(79);
        match(RccIRParser::LBRACK);
        setState(80);
        match(RccIRParser::NUMBER);
        setState(81);
        match(RccIRParser::RBRACK);
        setState(84);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == RccIRParser::ASSIGN) {
          setState(82);
          match(RccIRParser::ASSIGN);
          setState(83);
          initList();
        }
        break;
      }

      case RccIRParser::SEMI:
      case RccIRParser::ASSIGN: {
        setState(88);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == RccIRParser::ASSIGN) {
          setState(86);
          match(RccIRParser::ASSIGN);
          setState(87);
          match(RccIRParser::NUMBER);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(92);
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
    setState(94);
    match(RccIRParser::LBRACE);
    setState(103);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == RccIRParser::IDENT

    || _la == RccIRParser::NUMBER) {
      setState(95);
      initElem();
      setState(100);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == RccIRParser::COMMA) {
        setState(96);
        match(RccIRParser::COMMA);
        setState(97);
        initElem();
        setState(102);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(105);
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
    setState(109);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case RccIRParser::NUMBER: {
        enterOuterAlt(_localctx, 1);
        setState(107);
        match(RccIRParser::NUMBER);
        break;
      }

      case RccIRParser::IDENT: {
        enterOuterAlt(_localctx, 2);
        setState(108);
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
    setState(111);
    typeSpecOrVoid();
    setState(112);
    ref();
    setState(113);
    match(RccIRParser::LPAREN);
    setState(115);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(114);
      paramList();
    }
    setState(117);
    match(RccIRParser::RPAREN);
    setState(118);
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
    setState(122);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case RccIRParser::INT_KW:
      case RccIRParser::CHAR_KW:
      case RccIRParser::SHORT_KW:
      case RccIRParser::LONG_KW:
      case RccIRParser::UNSIGNED_KW: {
        enterOuterAlt(_localctx, 1);
        setState(120);
        typeSpec();
        break;
      }

      case RccIRParser::VOID_KW: {
        enterOuterAlt(_localctx, 2);
        setState(121);
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
    setState(124);
    param();
    setState(129);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == RccIRParser::COMMA) {
      setState(125);
      match(RccIRParser::COMMA);
      setState(126);
      param();
      setState(131);
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
    setState(132);
    typeSpec();
    setState(133);
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
    setState(135);
    match(RccIRParser::LBRACE);
    setState(139);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 9) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 9)) & 49539595933665019) != 0)) {
      setState(136);
      stmt();
      setState(141);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(142);
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
    setState(202);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 25, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<RccIRParser::BlockStmtContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(144);
      block();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<RccIRParser::IfStmtContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(145);
      match(RccIRParser::IF_KW);
      setState(146);
      match(RccIRParser::LPAREN);
      setState(147);
      expr();
      setState(148);
      match(RccIRParser::RPAREN);
      setState(149);
      stmt();
      setState(152);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 19, _ctx)) {
      case 1: {
        setState(150);
        match(RccIRParser::ELSE_KW);
        setState(151);
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
      setState(154);
      match(RccIRParser::WHILE_KW);
      setState(155);
      match(RccIRParser::LPAREN);
      setState(156);
      expr();
      setState(157);
      match(RccIRParser::RPAREN);
      setState(158);
      stmt();
      setState(161);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 20, _ctx)) {
      case 1: {
        setState(159);
        match(RccIRParser::POST_KW);
        setState(160);
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
      setState(163);
      match(RccIRParser::RETURN_KW);
      setState(165);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 6917529044284866560) != 0)) {
        setState(164);
        expr();
      }
      setState(167);
      match(RccIRParser::SEMI);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<RccIRParser::BreakStmtContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(168);
      match(RccIRParser::BREAK_KW);
      setState(169);
      match(RccIRParser::SEMI);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<RccIRParser::ContinueStmtContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(170);
      match(RccIRParser::CONTINUE_KW);
      setState(171);
      match(RccIRParser::SEMI);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<RccIRParser::LocalDeclStmtContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(172);
      match(RccIRParser::LOCAL_KW);
      setState(173);
      typeSpec();
      setState(174);
      ref();
      setState(186);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case RccIRParser::LBRACK: {
          setState(175);
          match(RccIRParser::LBRACK);
          setState(176);
          match(RccIRParser::NUMBER);
          setState(177);
          match(RccIRParser::RBRACK);
          setState(180);
          _errHandler->sync(this);

          _la = _input->LA(1);
          if (_la == RccIRParser::ASSIGN) {
            setState(178);
            match(RccIRParser::ASSIGN);
            setState(179);
            initList();
          }
          break;
        }

        case RccIRParser::SEMI:
        case RccIRParser::ASSIGN: {
          setState(184);
          _errHandler->sync(this);

          _la = _input->LA(1);
          if (_la == RccIRParser::ASSIGN) {
            setState(182);
            match(RccIRParser::ASSIGN);
            setState(183);
            expr();
          }
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(188);
      match(RccIRParser::SEMI);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<RccIRParser::AsmStmtContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(190);
      match(RccIRParser::ASM_BEGIN);
      setState(191);
      match(RccIRParser::ASM_TEXT);
      setState(192);
      match(RccIRParser::ASM_END);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<RccIRParser::EmptyStmtContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(193);
      match(RccIRParser::SEMI);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<RccIRParser::LabelStmtContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(194);
      match(RccIRParser::IDENT);
      setState(195);
      match(RccIRParser::COLON);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<RccIRParser::GotoStmtContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(196);
      match(RccIRParser::GOTO_KW);
      setState(197);
      match(RccIRParser::IDENT);
      setState(198);
      match(RccIRParser::SEMI);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<RccIRParser::ExprStmtContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(199);
      expr();
      setState(200);
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
    setState(204);
    match(RccIRParser::IDENT);
    setState(205);
    match(RccIRParser::HASH);
    setState(206);
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
    setState(208);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 575897802350002176) != 0))) {
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
    setState(301);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 32, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<RccIRParser::IntLitExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(210);
      match(RccIRParser::NUMBER);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<RccIRParser::CompoundIndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(212);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::OLDVAL_KW) {
        setState(211);
        match(RccIRParser::OLDVAL_KW);
      }
      setState(214);
      ref();
      setState(215);
      match(RccIRParser::LBRACK);
      setState(216);
      expr();
      setState(217);
      match(RccIRParser::RBRACK);
      setState(218);
      compoundOp();
      setState(219);
      expr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<RccIRParser::CompoundAssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(222);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::OLDVAL_KW) {
        setState(221);
        match(RccIRParser::OLDVAL_KW);
      }
      setState(224);
      ref();
      setState(225);
      compoundOp();
      setState(226);
      expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<RccIRParser::CompoundDerefExprContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(229);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::OLDVAL_KW) {
        setState(228);
        match(RccIRParser::OLDVAL_KW);
      }
      setState(231);
      match(RccIRParser::STAR);
      setState(232);
      match(RccIRParser::LPAREN);
      setState(233);
      expr();
      setState(234);
      match(RccIRParser::RPAREN);
      setState(235);
      compoundOp();
      setState(236);
      expr();
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<RccIRParser::IndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(238);
      ref();
      setState(239);
      match(RccIRParser::LBRACK);
      setState(240);
      expr();
      setState(241);
      match(RccIRParser::RBRACK);
      setState(244);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::ASSIGN) {
        setState(242);
        match(RccIRParser::ASSIGN);
        setState(243);
        expr();
      }
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<RccIRParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(246);
      ref();
      setState(247);
      match(RccIRParser::ASSIGN);
      setState(248);
      expr();
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<RccIRParser::StarExprContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(250);
      match(RccIRParser::STAR);
      setState(251);
      match(RccIRParser::LPAREN);
      setState(252);
      expr();
      setState(253);
      match(RccIRParser::RPAREN);
      setState(256);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccIRParser::ASSIGN) {
        setState(254);
        match(RccIRParser::ASSIGN);
        setState(255);
        expr();
      }
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<RccIRParser::UnaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(258);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 16106127360) != 0))) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(259);
      match(RccIRParser::LPAREN);
      setState(260);
      expr();
      setState(261);
      match(RccIRParser::RPAREN);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<RccIRParser::BinaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(263);
      match(RccIRParser::LPAREN);
      setState(264);
      expr();
      setState(265);
      binOp();
      setState(266);
      expr();
      setState(267);
      match(RccIRParser::RPAREN);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<RccIRParser::TernaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(269);
      match(RccIRParser::LPAREN);
      setState(270);
      expr();
      setState(271);
      match(RccIRParser::QUESTION);
      setState(272);
      expr();
      setState(273);
      match(RccIRParser::COLON);
      setState(274);
      expr();
      setState(275);
      match(RccIRParser::RPAREN);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<RccIRParser::ConvertExprContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(277);
      match(RccIRParser::LPAREN);
      setState(278);
      typeSpec();
      setState(279);
      match(RccIRParser::RPAREN);
      setState(280);
      match(RccIRParser::LPAREN);
      setState(281);
      expr();
      setState(282);
      match(RccIRParser::RPAREN);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<RccIRParser::CallExprContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(284);
      ref();
      setState(285);
      match(RccIRParser::LPAREN);
      setState(287);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 6917529044284866560) != 0)) {
        setState(286);
        argList();
      }
      setState(289);
      match(RccIRParser::RPAREN);
      break;
    }

    case 13: {
      _localctx = _tracker.createInstance<RccIRParser::CallPtrExprContext>(_localctx);
      enterOuterAlt(_localctx, 13);
      setState(291);
      match(RccIRParser::LPAREN);
      setState(292);
      expr();
      setState(293);
      match(RccIRParser::RPAREN);
      setState(294);
      match(RccIRParser::LPAREN);
      setState(295);
      match(RccIRParser::RPAREN);
      break;
    }

    case 14: {
      _localctx = _tracker.createInstance<RccIRParser::IntrinsicExprContext>(_localctx);
      enterOuterAlt(_localctx, 14);
      setState(297);
      match(RccIRParser::IDENT);
      setState(298);
      match(RccIRParser::LPAREN);
      setState(299);
      match(RccIRParser::RPAREN);
      break;
    }

    case 15: {
      _localctx = _tracker.createInstance<RccIRParser::VarExprContext>(_localctx);
      enterOuterAlt(_localctx, 15);
      setState(300);
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
    setState(303);
    expr();
    setState(308);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == RccIRParser::COMMA) {
      setState(304);
      match(RccIRParser::COMMA);
      setState(305);
      expr();
      setState(310);
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
    setState(311);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 562936531648512) != 0))) {
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
