
// Generated from /Users/tom/projects/pcc/grammar/PccIRParser.g4 by ANTLR 4.13.2


#include "PccIRParserVisitor.h"

#include "PccIRParser.h"


using namespace antlrcpp;

using namespace antlr4;

namespace {

struct PccIRParserStaticData final {
  PccIRParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  PccIRParserStaticData(const PccIRParserStaticData&) = delete;
  PccIRParserStaticData(PccIRParserStaticData&&) = delete;
  PccIRParserStaticData& operator=(const PccIRParserStaticData&) = delete;
  PccIRParserStaticData& operator=(PccIRParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag pccirparserParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<PccIRParserStaticData> pccirparserParserStaticData = nullptr;

void pccirparserParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (pccirparserParserStaticData != nullptr) {
    return;
  }
#else
  assert(pccirparserParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<PccIRParserStaticData>(
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
  pccirparserParserStaticData = std::move(staticData);
}

}

PccIRParser::PccIRParser(TokenStream *input) : PccIRParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

PccIRParser::PccIRParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  PccIRParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *pccirparserParserStaticData->atn, pccirparserParserStaticData->decisionToDFA, pccirparserParserStaticData->sharedContextCache, options);
}

PccIRParser::~PccIRParser() {
  delete _interpreter;
}

const atn::ATN& PccIRParser::getATN() const {
  return *pccirparserParserStaticData->atn;
}

std::string PccIRParser::getGrammarFileName() const {
  return "PccIRParser.g4";
}

const std::vector<std::string>& PccIRParser::getRuleNames() const {
  return pccirparserParserStaticData->ruleNames;
}

const dfa::Vocabulary& PccIRParser::getVocabulary() const {
  return pccirparserParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView PccIRParser::getSerializedATN() const {
  return pccirparserParserStaticData->serializedATN;
}


//----------------- ProgramContext ------------------------------------------------------------------

PccIRParser::ProgramContext::ProgramContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccIRParser::ProgramContext::GLOBALS_HEADER() {
  return getToken(PccIRParser::GLOBALS_HEADER, 0);
}

tree::TerminalNode* PccIRParser::ProgramContext::FUNCTIONS_HEADER() {
  return getToken(PccIRParser::FUNCTIONS_HEADER, 0);
}

tree::TerminalNode* PccIRParser::ProgramContext::EOF() {
  return getToken(PccIRParser::EOF, 0);
}

std::vector<PccIRParser::GlobalDeclContext *> PccIRParser::ProgramContext::globalDecl() {
  return getRuleContexts<PccIRParser::GlobalDeclContext>();
}

PccIRParser::GlobalDeclContext* PccIRParser::ProgramContext::globalDecl(size_t i) {
  return getRuleContext<PccIRParser::GlobalDeclContext>(i);
}

std::vector<PccIRParser::FuncDeclContext *> PccIRParser::ProgramContext::funcDecl() {
  return getRuleContexts<PccIRParser::FuncDeclContext>();
}

PccIRParser::FuncDeclContext* PccIRParser::ProgramContext::funcDecl(size_t i) {
  return getRuleContext<PccIRParser::FuncDeclContext>(i);
}


size_t PccIRParser::ProgramContext::getRuleIndex() const {
  return PccIRParser::RuleProgram;
}


std::any PccIRParser::ProgramContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitProgram(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::ProgramContext* PccIRParser::program() {
  ProgramContext *_localctx = _tracker.createInstance<ProgramContext>(_ctx, getState());
  enterRule(_localctx, 0, PccIRParser::RuleProgram);
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
    match(PccIRParser::GLOBALS_HEADER);
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
    match(PccIRParser::FUNCTIONS_HEADER);
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
    match(PccIRParser::EOF);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TypeSpecContext ------------------------------------------------------------------

PccIRParser::TypeSpecContext::TypeSpecContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccIRParser::TypeSpecContext::CHAR_KW() {
  return getToken(PccIRParser::CHAR_KW, 0);
}

tree::TerminalNode* PccIRParser::TypeSpecContext::SHORT_KW() {
  return getToken(PccIRParser::SHORT_KW, 0);
}

tree::TerminalNode* PccIRParser::TypeSpecContext::INT_KW() {
  return getToken(PccIRParser::INT_KW, 0);
}

std::vector<tree::TerminalNode *> PccIRParser::TypeSpecContext::LONG_KW() {
  return getTokens(PccIRParser::LONG_KW);
}

tree::TerminalNode* PccIRParser::TypeSpecContext::LONG_KW(size_t i) {
  return getToken(PccIRParser::LONG_KW, i);
}

std::vector<tree::TerminalNode *> PccIRParser::TypeSpecContext::STAR() {
  return getTokens(PccIRParser::STAR);
}

tree::TerminalNode* PccIRParser::TypeSpecContext::STAR(size_t i) {
  return getToken(PccIRParser::STAR, i);
}

tree::TerminalNode* PccIRParser::TypeSpecContext::UNSIGNED_KW() {
  return getToken(PccIRParser::UNSIGNED_KW, 0);
}


size_t PccIRParser::TypeSpecContext::getRuleIndex() const {
  return PccIRParser::RuleTypeSpec;
}


std::any PccIRParser::TypeSpecContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitTypeSpec(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::TypeSpecContext* PccIRParser::typeSpec() {
  TypeSpecContext *_localctx = _tracker.createInstance<TypeSpecContext>(_ctx, getState());
  enterRule(_localctx, 2, PccIRParser::RuleTypeSpec);
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
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(48);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(51);
      match(PccIRParser::CHAR_KW);
      break;
    }

    case 2: {
      setState(53);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(52);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(55);
      match(PccIRParser::SHORT_KW);
      break;
    }

    case 3: {
      setState(57);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(56);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(59);
      match(PccIRParser::INT_KW);
      break;
    }

    case 4: {
      setState(61);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(60);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(63);
      match(PccIRParser::LONG_KW);
      setState(64);
      match(PccIRParser::LONG_KW);
      break;
    }

    case 5: {
      setState(66);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(65);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(68);
      match(PccIRParser::LONG_KW);
      break;
    }

    default:
      break;
    }
    setState(74);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::STAR) {
      setState(71);
      match(PccIRParser::STAR);
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

PccIRParser::GlobalDeclContext::GlobalDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

PccIRParser::TypeSpecContext* PccIRParser::GlobalDeclContext::typeSpec() {
  return getRuleContext<PccIRParser::TypeSpecContext>(0);
}

PccIRParser::RefContext* PccIRParser::GlobalDeclContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

tree::TerminalNode* PccIRParser::GlobalDeclContext::SEMI() {
  return getToken(PccIRParser::SEMI, 0);
}

tree::TerminalNode* PccIRParser::GlobalDeclContext::LBRACK() {
  return getToken(PccIRParser::LBRACK, 0);
}

tree::TerminalNode* PccIRParser::GlobalDeclContext::NUMBER() {
  return getToken(PccIRParser::NUMBER, 0);
}

tree::TerminalNode* PccIRParser::GlobalDeclContext::RBRACK() {
  return getToken(PccIRParser::RBRACK, 0);
}

tree::TerminalNode* PccIRParser::GlobalDeclContext::ASSIGN() {
  return getToken(PccIRParser::ASSIGN, 0);
}

PccIRParser::InitListContext* PccIRParser::GlobalDeclContext::initList() {
  return getRuleContext<PccIRParser::InitListContext>(0);
}


size_t PccIRParser::GlobalDeclContext::getRuleIndex() const {
  return PccIRParser::RuleGlobalDecl;
}


std::any PccIRParser::GlobalDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitGlobalDecl(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::GlobalDeclContext* PccIRParser::globalDecl() {
  GlobalDeclContext *_localctx = _tracker.createInstance<GlobalDeclContext>(_ctx, getState());
  enterRule(_localctx, 4, PccIRParser::RuleGlobalDecl);
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
      case PccIRParser::LBRACK: {
        setState(79);
        match(PccIRParser::LBRACK);
        setState(80);
        match(PccIRParser::NUMBER);
        setState(81);
        match(PccIRParser::RBRACK);
        setState(84);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(82);
          match(PccIRParser::ASSIGN);
          setState(83);
          initList();
        }
        break;
      }

      case PccIRParser::SEMI:
      case PccIRParser::ASSIGN: {
        setState(88);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(86);
          match(PccIRParser::ASSIGN);
          setState(87);
          match(PccIRParser::NUMBER);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(92);
    match(PccIRParser::SEMI);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- InitListContext ------------------------------------------------------------------

PccIRParser::InitListContext::InitListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccIRParser::InitListContext::LBRACE() {
  return getToken(PccIRParser::LBRACE, 0);
}

tree::TerminalNode* PccIRParser::InitListContext::RBRACE() {
  return getToken(PccIRParser::RBRACE, 0);
}

std::vector<PccIRParser::InitElemContext *> PccIRParser::InitListContext::initElem() {
  return getRuleContexts<PccIRParser::InitElemContext>();
}

PccIRParser::InitElemContext* PccIRParser::InitListContext::initElem(size_t i) {
  return getRuleContext<PccIRParser::InitElemContext>(i);
}

std::vector<tree::TerminalNode *> PccIRParser::InitListContext::COMMA() {
  return getTokens(PccIRParser::COMMA);
}

tree::TerminalNode* PccIRParser::InitListContext::COMMA(size_t i) {
  return getToken(PccIRParser::COMMA, i);
}


size_t PccIRParser::InitListContext::getRuleIndex() const {
  return PccIRParser::RuleInitList;
}


std::any PccIRParser::InitListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitInitList(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::InitListContext* PccIRParser::initList() {
  InitListContext *_localctx = _tracker.createInstance<InitListContext>(_ctx, getState());
  enterRule(_localctx, 6, PccIRParser::RuleInitList);
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
    match(PccIRParser::LBRACE);
    setState(103);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == PccIRParser::IDENT

    || _la == PccIRParser::NUMBER) {
      setState(95);
      initElem();
      setState(100);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == PccIRParser::COMMA) {
        setState(96);
        match(PccIRParser::COMMA);
        setState(97);
        initElem();
        setState(102);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(105);
    match(PccIRParser::RBRACE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- InitElemContext ------------------------------------------------------------------

PccIRParser::InitElemContext::InitElemContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccIRParser::InitElemContext::NUMBER() {
  return getToken(PccIRParser::NUMBER, 0);
}

PccIRParser::RefContext* PccIRParser::InitElemContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}


size_t PccIRParser::InitElemContext::getRuleIndex() const {
  return PccIRParser::RuleInitElem;
}


std::any PccIRParser::InitElemContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitInitElem(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::InitElemContext* PccIRParser::initElem() {
  InitElemContext *_localctx = _tracker.createInstance<InitElemContext>(_ctx, getState());
  enterRule(_localctx, 8, PccIRParser::RuleInitElem);

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
      case PccIRParser::NUMBER: {
        enterOuterAlt(_localctx, 1);
        setState(107);
        match(PccIRParser::NUMBER);
        break;
      }

      case PccIRParser::IDENT: {
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

PccIRParser::FuncDeclContext::FuncDeclContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

PccIRParser::TypeSpecOrVoidContext* PccIRParser::FuncDeclContext::typeSpecOrVoid() {
  return getRuleContext<PccIRParser::TypeSpecOrVoidContext>(0);
}

PccIRParser::RefContext* PccIRParser::FuncDeclContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

tree::TerminalNode* PccIRParser::FuncDeclContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

tree::TerminalNode* PccIRParser::FuncDeclContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

PccIRParser::BlockContext* PccIRParser::FuncDeclContext::block() {
  return getRuleContext<PccIRParser::BlockContext>(0);
}

PccIRParser::ParamListContext* PccIRParser::FuncDeclContext::paramList() {
  return getRuleContext<PccIRParser::ParamListContext>(0);
}


size_t PccIRParser::FuncDeclContext::getRuleIndex() const {
  return PccIRParser::RuleFuncDecl;
}


std::any PccIRParser::FuncDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitFuncDecl(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::FuncDeclContext* PccIRParser::funcDecl() {
  FuncDeclContext *_localctx = _tracker.createInstance<FuncDeclContext>(_ctx, getState());
  enterRule(_localctx, 10, PccIRParser::RuleFuncDecl);
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
    match(PccIRParser::LPAREN);
    setState(115);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(114);
      paramList();
    }
    setState(117);
    match(PccIRParser::RPAREN);
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

PccIRParser::TypeSpecOrVoidContext::TypeSpecOrVoidContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

PccIRParser::TypeSpecContext* PccIRParser::TypeSpecOrVoidContext::typeSpec() {
  return getRuleContext<PccIRParser::TypeSpecContext>(0);
}

tree::TerminalNode* PccIRParser::TypeSpecOrVoidContext::VOID_KW() {
  return getToken(PccIRParser::VOID_KW, 0);
}


size_t PccIRParser::TypeSpecOrVoidContext::getRuleIndex() const {
  return PccIRParser::RuleTypeSpecOrVoid;
}


std::any PccIRParser::TypeSpecOrVoidContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitTypeSpecOrVoid(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::TypeSpecOrVoidContext* PccIRParser::typeSpecOrVoid() {
  TypeSpecOrVoidContext *_localctx = _tracker.createInstance<TypeSpecOrVoidContext>(_ctx, getState());
  enterRule(_localctx, 12, PccIRParser::RuleTypeSpecOrVoid);

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
      case PccIRParser::INT_KW:
      case PccIRParser::CHAR_KW:
      case PccIRParser::SHORT_KW:
      case PccIRParser::LONG_KW:
      case PccIRParser::UNSIGNED_KW: {
        enterOuterAlt(_localctx, 1);
        setState(120);
        typeSpec();
        break;
      }

      case PccIRParser::VOID_KW: {
        enterOuterAlt(_localctx, 2);
        setState(121);
        match(PccIRParser::VOID_KW);
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

PccIRParser::ParamListContext::ParamListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<PccIRParser::ParamContext *> PccIRParser::ParamListContext::param() {
  return getRuleContexts<PccIRParser::ParamContext>();
}

PccIRParser::ParamContext* PccIRParser::ParamListContext::param(size_t i) {
  return getRuleContext<PccIRParser::ParamContext>(i);
}

std::vector<tree::TerminalNode *> PccIRParser::ParamListContext::COMMA() {
  return getTokens(PccIRParser::COMMA);
}

tree::TerminalNode* PccIRParser::ParamListContext::COMMA(size_t i) {
  return getToken(PccIRParser::COMMA, i);
}


size_t PccIRParser::ParamListContext::getRuleIndex() const {
  return PccIRParser::RuleParamList;
}


std::any PccIRParser::ParamListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitParamList(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::ParamListContext* PccIRParser::paramList() {
  ParamListContext *_localctx = _tracker.createInstance<ParamListContext>(_ctx, getState());
  enterRule(_localctx, 14, PccIRParser::RuleParamList);
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
    while (_la == PccIRParser::COMMA) {
      setState(125);
      match(PccIRParser::COMMA);
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

PccIRParser::ParamContext::ParamContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

PccIRParser::TypeSpecContext* PccIRParser::ParamContext::typeSpec() {
  return getRuleContext<PccIRParser::TypeSpecContext>(0);
}

PccIRParser::RefContext* PccIRParser::ParamContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}


size_t PccIRParser::ParamContext::getRuleIndex() const {
  return PccIRParser::RuleParam;
}


std::any PccIRParser::ParamContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitParam(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::ParamContext* PccIRParser::param() {
  ParamContext *_localctx = _tracker.createInstance<ParamContext>(_ctx, getState());
  enterRule(_localctx, 16, PccIRParser::RuleParam);

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

PccIRParser::BlockContext::BlockContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccIRParser::BlockContext::LBRACE() {
  return getToken(PccIRParser::LBRACE, 0);
}

tree::TerminalNode* PccIRParser::BlockContext::RBRACE() {
  return getToken(PccIRParser::RBRACE, 0);
}

std::vector<PccIRParser::StmtContext *> PccIRParser::BlockContext::stmt() {
  return getRuleContexts<PccIRParser::StmtContext>();
}

PccIRParser::StmtContext* PccIRParser::BlockContext::stmt(size_t i) {
  return getRuleContext<PccIRParser::StmtContext>(i);
}


size_t PccIRParser::BlockContext::getRuleIndex() const {
  return PccIRParser::RuleBlock;
}


std::any PccIRParser::BlockContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitBlock(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::BlockContext* PccIRParser::block() {
  BlockContext *_localctx = _tracker.createInstance<BlockContext>(_ctx, getState());
  enterRule(_localctx, 18, PccIRParser::RuleBlock);
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
    match(PccIRParser::LBRACE);
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
    match(PccIRParser::RBRACE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StmtContext ------------------------------------------------------------------

PccIRParser::StmtContext::StmtContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t PccIRParser::StmtContext::getRuleIndex() const {
  return PccIRParser::RuleStmt;
}

void PccIRParser::StmtContext::copyFrom(StmtContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- LocalDeclStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::LocalDeclStmtContext::LOCAL_KW() {
  return getToken(PccIRParser::LOCAL_KW, 0);
}

PccIRParser::TypeSpecContext* PccIRParser::LocalDeclStmtContext::typeSpec() {
  return getRuleContext<PccIRParser::TypeSpecContext>(0);
}

PccIRParser::RefContext* PccIRParser::LocalDeclStmtContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

tree::TerminalNode* PccIRParser::LocalDeclStmtContext::SEMI() {
  return getToken(PccIRParser::SEMI, 0);
}

tree::TerminalNode* PccIRParser::LocalDeclStmtContext::LBRACK() {
  return getToken(PccIRParser::LBRACK, 0);
}

tree::TerminalNode* PccIRParser::LocalDeclStmtContext::NUMBER() {
  return getToken(PccIRParser::NUMBER, 0);
}

tree::TerminalNode* PccIRParser::LocalDeclStmtContext::RBRACK() {
  return getToken(PccIRParser::RBRACK, 0);
}

tree::TerminalNode* PccIRParser::LocalDeclStmtContext::ASSIGN() {
  return getToken(PccIRParser::ASSIGN, 0);
}

PccIRParser::InitListContext* PccIRParser::LocalDeclStmtContext::initList() {
  return getRuleContext<PccIRParser::InitListContext>(0);
}

PccIRParser::ExprContext* PccIRParser::LocalDeclStmtContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

PccIRParser::LocalDeclStmtContext::LocalDeclStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::LocalDeclStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitLocalDeclStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AsmStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::AsmStmtContext::ASM_BEGIN() {
  return getToken(PccIRParser::ASM_BEGIN, 0);
}

tree::TerminalNode* PccIRParser::AsmStmtContext::ASM_TEXT() {
  return getToken(PccIRParser::ASM_TEXT, 0);
}

tree::TerminalNode* PccIRParser::AsmStmtContext::ASM_END() {
  return getToken(PccIRParser::ASM_END, 0);
}

PccIRParser::AsmStmtContext::AsmStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::AsmStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitAsmStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ExprStmtContext ------------------------------------------------------------------

PccIRParser::ExprContext* PccIRParser::ExprStmtContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

tree::TerminalNode* PccIRParser::ExprStmtContext::SEMI() {
  return getToken(PccIRParser::SEMI, 0);
}

PccIRParser::ExprStmtContext::ExprStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::ExprStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitExprStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- WhileStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::WhileStmtContext::WHILE_KW() {
  return getToken(PccIRParser::WHILE_KW, 0);
}

tree::TerminalNode* PccIRParser::WhileStmtContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

PccIRParser::ExprContext* PccIRParser::WhileStmtContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

tree::TerminalNode* PccIRParser::WhileStmtContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

std::vector<PccIRParser::StmtContext *> PccIRParser::WhileStmtContext::stmt() {
  return getRuleContexts<PccIRParser::StmtContext>();
}

PccIRParser::StmtContext* PccIRParser::WhileStmtContext::stmt(size_t i) {
  return getRuleContext<PccIRParser::StmtContext>(i);
}

tree::TerminalNode* PccIRParser::WhileStmtContext::POST_KW() {
  return getToken(PccIRParser::POST_KW, 0);
}

PccIRParser::WhileStmtContext::WhileStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::WhileStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitWhileStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IfStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::IfStmtContext::IF_KW() {
  return getToken(PccIRParser::IF_KW, 0);
}

tree::TerminalNode* PccIRParser::IfStmtContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

PccIRParser::ExprContext* PccIRParser::IfStmtContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

tree::TerminalNode* PccIRParser::IfStmtContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

std::vector<PccIRParser::StmtContext *> PccIRParser::IfStmtContext::stmt() {
  return getRuleContexts<PccIRParser::StmtContext>();
}

PccIRParser::StmtContext* PccIRParser::IfStmtContext::stmt(size_t i) {
  return getRuleContext<PccIRParser::StmtContext>(i);
}

tree::TerminalNode* PccIRParser::IfStmtContext::ELSE_KW() {
  return getToken(PccIRParser::ELSE_KW, 0);
}

PccIRParser::IfStmtContext::IfStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::IfStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitIfStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BlockStmtContext ------------------------------------------------------------------

PccIRParser::BlockContext* PccIRParser::BlockStmtContext::block() {
  return getRuleContext<PccIRParser::BlockContext>(0);
}

PccIRParser::BlockStmtContext::BlockStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::BlockStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitBlockStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BreakStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::BreakStmtContext::BREAK_KW() {
  return getToken(PccIRParser::BREAK_KW, 0);
}

tree::TerminalNode* PccIRParser::BreakStmtContext::SEMI() {
  return getToken(PccIRParser::SEMI, 0);
}

PccIRParser::BreakStmtContext::BreakStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::BreakStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitBreakStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- EmptyStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::EmptyStmtContext::SEMI() {
  return getToken(PccIRParser::SEMI, 0);
}

PccIRParser::EmptyStmtContext::EmptyStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::EmptyStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitEmptyStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- GotoStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::GotoStmtContext::GOTO_KW() {
  return getToken(PccIRParser::GOTO_KW, 0);
}

tree::TerminalNode* PccIRParser::GotoStmtContext::IDENT() {
  return getToken(PccIRParser::IDENT, 0);
}

tree::TerminalNode* PccIRParser::GotoStmtContext::SEMI() {
  return getToken(PccIRParser::SEMI, 0);
}

PccIRParser::GotoStmtContext::GotoStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::GotoStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitGotoStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- LabelStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::LabelStmtContext::IDENT() {
  return getToken(PccIRParser::IDENT, 0);
}

tree::TerminalNode* PccIRParser::LabelStmtContext::COLON() {
  return getToken(PccIRParser::COLON, 0);
}

PccIRParser::LabelStmtContext::LabelStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::LabelStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitLabelStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ReturnStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::ReturnStmtContext::RETURN_KW() {
  return getToken(PccIRParser::RETURN_KW, 0);
}

tree::TerminalNode* PccIRParser::ReturnStmtContext::SEMI() {
  return getToken(PccIRParser::SEMI, 0);
}

PccIRParser::ExprContext* PccIRParser::ReturnStmtContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

PccIRParser::ReturnStmtContext::ReturnStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::ReturnStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitReturnStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ContinueStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::ContinueStmtContext::CONTINUE_KW() {
  return getToken(PccIRParser::CONTINUE_KW, 0);
}

tree::TerminalNode* PccIRParser::ContinueStmtContext::SEMI() {
  return getToken(PccIRParser::SEMI, 0);
}

PccIRParser::ContinueStmtContext::ContinueStmtContext(StmtContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::ContinueStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitContinueStmt(this);
  else
    return visitor->visitChildren(this);
}
PccIRParser::StmtContext* PccIRParser::stmt() {
  StmtContext *_localctx = _tracker.createInstance<StmtContext>(_ctx, getState());
  enterRule(_localctx, 20, PccIRParser::RuleStmt);
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
      _localctx = _tracker.createInstance<PccIRParser::BlockStmtContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(144);
      block();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PccIRParser::IfStmtContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(145);
      match(PccIRParser::IF_KW);
      setState(146);
      match(PccIRParser::LPAREN);
      setState(147);
      expr();
      setState(148);
      match(PccIRParser::RPAREN);
      setState(149);
      stmt();
      setState(152);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 19, _ctx)) {
      case 1: {
        setState(150);
        match(PccIRParser::ELSE_KW);
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
      _localctx = _tracker.createInstance<PccIRParser::WhileStmtContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(154);
      match(PccIRParser::WHILE_KW);
      setState(155);
      match(PccIRParser::LPAREN);
      setState(156);
      expr();
      setState(157);
      match(PccIRParser::RPAREN);
      setState(158);
      stmt();
      setState(161);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 20, _ctx)) {
      case 1: {
        setState(159);
        match(PccIRParser::POST_KW);
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
      _localctx = _tracker.createInstance<PccIRParser::ReturnStmtContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(163);
      match(PccIRParser::RETURN_KW);
      setState(165);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 6917529044284866560) != 0)) {
        setState(164);
        expr();
      }
      setState(167);
      match(PccIRParser::SEMI);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<PccIRParser::BreakStmtContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(168);
      match(PccIRParser::BREAK_KW);
      setState(169);
      match(PccIRParser::SEMI);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<PccIRParser::ContinueStmtContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(170);
      match(PccIRParser::CONTINUE_KW);
      setState(171);
      match(PccIRParser::SEMI);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<PccIRParser::LocalDeclStmtContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(172);
      match(PccIRParser::LOCAL_KW);
      setState(173);
      typeSpec();
      setState(174);
      ref();
      setState(186);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case PccIRParser::LBRACK: {
          setState(175);
          match(PccIRParser::LBRACK);
          setState(176);
          match(PccIRParser::NUMBER);
          setState(177);
          match(PccIRParser::RBRACK);
          setState(180);
          _errHandler->sync(this);

          _la = _input->LA(1);
          if (_la == PccIRParser::ASSIGN) {
            setState(178);
            match(PccIRParser::ASSIGN);
            setState(179);
            initList();
          }
          break;
        }

        case PccIRParser::SEMI:
        case PccIRParser::ASSIGN: {
          setState(184);
          _errHandler->sync(this);

          _la = _input->LA(1);
          if (_la == PccIRParser::ASSIGN) {
            setState(182);
            match(PccIRParser::ASSIGN);
            setState(183);
            expr();
          }
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(188);
      match(PccIRParser::SEMI);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<PccIRParser::AsmStmtContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(190);
      match(PccIRParser::ASM_BEGIN);
      setState(191);
      match(PccIRParser::ASM_TEXT);
      setState(192);
      match(PccIRParser::ASM_END);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<PccIRParser::EmptyStmtContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(193);
      match(PccIRParser::SEMI);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<PccIRParser::LabelStmtContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(194);
      match(PccIRParser::IDENT);
      setState(195);
      match(PccIRParser::COLON);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<PccIRParser::GotoStmtContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(196);
      match(PccIRParser::GOTO_KW);
      setState(197);
      match(PccIRParser::IDENT);
      setState(198);
      match(PccIRParser::SEMI);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<PccIRParser::ExprStmtContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(199);
      expr();
      setState(200);
      match(PccIRParser::SEMI);
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

PccIRParser::RefContext::RefContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccIRParser::RefContext::IDENT() {
  return getToken(PccIRParser::IDENT, 0);
}

tree::TerminalNode* PccIRParser::RefContext::HASH() {
  return getToken(PccIRParser::HASH, 0);
}

tree::TerminalNode* PccIRParser::RefContext::NUMBER() {
  return getToken(PccIRParser::NUMBER, 0);
}


size_t PccIRParser::RefContext::getRuleIndex() const {
  return PccIRParser::RuleRef;
}


std::any PccIRParser::RefContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitRef(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::RefContext* PccIRParser::ref() {
  RefContext *_localctx = _tracker.createInstance<RefContext>(_ctx, getState());
  enterRule(_localctx, 22, PccIRParser::RuleRef);

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
    match(PccIRParser::IDENT);
    setState(205);
    match(PccIRParser::HASH);
    setState(206);
    match(PccIRParser::NUMBER);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CompoundOpContext ------------------------------------------------------------------

PccIRParser::CompoundOpContext::CompoundOpContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccIRParser::CompoundOpContext::PLUSEQ() {
  return getToken(PccIRParser::PLUSEQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::MINUSEQ() {
  return getToken(PccIRParser::MINUSEQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::STAREQ() {
  return getToken(PccIRParser::STAREQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::SLASHEQ() {
  return getToken(PccIRParser::SLASHEQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::PERCENTEQ() {
  return getToken(PccIRParser::PERCENTEQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::AMPEQ() {
  return getToken(PccIRParser::AMPEQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::CARETEQ() {
  return getToken(PccIRParser::CARETEQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::PIPEEQ() {
  return getToken(PccIRParser::PIPEEQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::SHLEQ() {
  return getToken(PccIRParser::SHLEQ, 0);
}

tree::TerminalNode* PccIRParser::CompoundOpContext::SHREQ() {
  return getToken(PccIRParser::SHREQ, 0);
}


size_t PccIRParser::CompoundOpContext::getRuleIndex() const {
  return PccIRParser::RuleCompoundOp;
}


std::any PccIRParser::CompoundOpContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitCompoundOp(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::CompoundOpContext* PccIRParser::compoundOp() {
  CompoundOpContext *_localctx = _tracker.createInstance<CompoundOpContext>(_ctx, getState());
  enterRule(_localctx, 24, PccIRParser::RuleCompoundOp);
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

PccIRParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t PccIRParser::ExprContext::getRuleIndex() const {
  return PccIRParser::RuleExpr;
}

void PccIRParser::ExprContext::copyFrom(ExprContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- CompoundDerefExprContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::CompoundDerefExprContext::STAR() {
  return getToken(PccIRParser::STAR, 0);
}

tree::TerminalNode* PccIRParser::CompoundDerefExprContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

std::vector<PccIRParser::ExprContext *> PccIRParser::CompoundDerefExprContext::expr() {
  return getRuleContexts<PccIRParser::ExprContext>();
}

PccIRParser::ExprContext* PccIRParser::CompoundDerefExprContext::expr(size_t i) {
  return getRuleContext<PccIRParser::ExprContext>(i);
}

tree::TerminalNode* PccIRParser::CompoundDerefExprContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

PccIRParser::CompoundOpContext* PccIRParser::CompoundDerefExprContext::compoundOp() {
  return getRuleContext<PccIRParser::CompoundOpContext>(0);
}

tree::TerminalNode* PccIRParser::CompoundDerefExprContext::OLDVAL_KW() {
  return getToken(PccIRParser::OLDVAL_KW, 0);
}

PccIRParser::CompoundDerefExprContext::CompoundDerefExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::CompoundDerefExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitCompoundDerefExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- StarExprContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::StarExprContext::STAR() {
  return getToken(PccIRParser::STAR, 0);
}

tree::TerminalNode* PccIRParser::StarExprContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

std::vector<PccIRParser::ExprContext *> PccIRParser::StarExprContext::expr() {
  return getRuleContexts<PccIRParser::ExprContext>();
}

PccIRParser::ExprContext* PccIRParser::StarExprContext::expr(size_t i) {
  return getRuleContext<PccIRParser::ExprContext>(i);
}

tree::TerminalNode* PccIRParser::StarExprContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

tree::TerminalNode* PccIRParser::StarExprContext::ASSIGN() {
  return getToken(PccIRParser::ASSIGN, 0);
}

PccIRParser::StarExprContext::StarExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::StarExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitStarExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CompoundAssignExprContext ------------------------------------------------------------------

PccIRParser::RefContext* PccIRParser::CompoundAssignExprContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

PccIRParser::CompoundOpContext* PccIRParser::CompoundAssignExprContext::compoundOp() {
  return getRuleContext<PccIRParser::CompoundOpContext>(0);
}

PccIRParser::ExprContext* PccIRParser::CompoundAssignExprContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

tree::TerminalNode* PccIRParser::CompoundAssignExprContext::OLDVAL_KW() {
  return getToken(PccIRParser::OLDVAL_KW, 0);
}

PccIRParser::CompoundAssignExprContext::CompoundAssignExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::CompoundAssignExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitCompoundAssignExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IntLitExprContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::IntLitExprContext::NUMBER() {
  return getToken(PccIRParser::NUMBER, 0);
}

PccIRParser::IntLitExprContext::IntLitExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::IntLitExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitIntLitExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- BinaryExprContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::BinaryExprContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

std::vector<PccIRParser::ExprContext *> PccIRParser::BinaryExprContext::expr() {
  return getRuleContexts<PccIRParser::ExprContext>();
}

PccIRParser::ExprContext* PccIRParser::BinaryExprContext::expr(size_t i) {
  return getRuleContext<PccIRParser::ExprContext>(i);
}

PccIRParser::BinOpContext* PccIRParser::BinaryExprContext::binOp() {
  return getRuleContext<PccIRParser::BinOpContext>(0);
}

tree::TerminalNode* PccIRParser::BinaryExprContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

PccIRParser::BinaryExprContext::BinaryExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::BinaryExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitBinaryExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CallPtrExprContext ------------------------------------------------------------------

std::vector<tree::TerminalNode *> PccIRParser::CallPtrExprContext::LPAREN() {
  return getTokens(PccIRParser::LPAREN);
}

tree::TerminalNode* PccIRParser::CallPtrExprContext::LPAREN(size_t i) {
  return getToken(PccIRParser::LPAREN, i);
}

PccIRParser::ExprContext* PccIRParser::CallPtrExprContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

std::vector<tree::TerminalNode *> PccIRParser::CallPtrExprContext::RPAREN() {
  return getTokens(PccIRParser::RPAREN);
}

tree::TerminalNode* PccIRParser::CallPtrExprContext::RPAREN(size_t i) {
  return getToken(PccIRParser::RPAREN, i);
}

PccIRParser::CallPtrExprContext::CallPtrExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::CallPtrExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitCallPtrExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IndexExprContext ------------------------------------------------------------------

PccIRParser::RefContext* PccIRParser::IndexExprContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

tree::TerminalNode* PccIRParser::IndexExprContext::LBRACK() {
  return getToken(PccIRParser::LBRACK, 0);
}

std::vector<PccIRParser::ExprContext *> PccIRParser::IndexExprContext::expr() {
  return getRuleContexts<PccIRParser::ExprContext>();
}

PccIRParser::ExprContext* PccIRParser::IndexExprContext::expr(size_t i) {
  return getRuleContext<PccIRParser::ExprContext>(i);
}

tree::TerminalNode* PccIRParser::IndexExprContext::RBRACK() {
  return getToken(PccIRParser::RBRACK, 0);
}

tree::TerminalNode* PccIRParser::IndexExprContext::ASSIGN() {
  return getToken(PccIRParser::ASSIGN, 0);
}

PccIRParser::IndexExprContext::IndexExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::IndexExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitIndexExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- VarExprContext ------------------------------------------------------------------

PccIRParser::RefContext* PccIRParser::VarExprContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

PccIRParser::VarExprContext::VarExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::VarExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitVarExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- IntrinsicExprContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::IntrinsicExprContext::IDENT() {
  return getToken(PccIRParser::IDENT, 0);
}

tree::TerminalNode* PccIRParser::IntrinsicExprContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

tree::TerminalNode* PccIRParser::IntrinsicExprContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

PccIRParser::IntrinsicExprContext::IntrinsicExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::IntrinsicExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitIntrinsicExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- UnaryExprContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::UnaryExprContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

PccIRParser::ExprContext* PccIRParser::UnaryExprContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

tree::TerminalNode* PccIRParser::UnaryExprContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

tree::TerminalNode* PccIRParser::UnaryExprContext::MINUS() {
  return getToken(PccIRParser::MINUS, 0);
}

tree::TerminalNode* PccIRParser::UnaryExprContext::BANG() {
  return getToken(PccIRParser::BANG, 0);
}

tree::TerminalNode* PccIRParser::UnaryExprContext::TILDE() {
  return getToken(PccIRParser::TILDE, 0);
}

tree::TerminalNode* PccIRParser::UnaryExprContext::AMP() {
  return getToken(PccIRParser::AMP, 0);
}

PccIRParser::UnaryExprContext::UnaryExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::UnaryExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitUnaryExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- TernaryExprContext ------------------------------------------------------------------

tree::TerminalNode* PccIRParser::TernaryExprContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

std::vector<PccIRParser::ExprContext *> PccIRParser::TernaryExprContext::expr() {
  return getRuleContexts<PccIRParser::ExprContext>();
}

PccIRParser::ExprContext* PccIRParser::TernaryExprContext::expr(size_t i) {
  return getRuleContext<PccIRParser::ExprContext>(i);
}

tree::TerminalNode* PccIRParser::TernaryExprContext::QUESTION() {
  return getToken(PccIRParser::QUESTION, 0);
}

tree::TerminalNode* PccIRParser::TernaryExprContext::COLON() {
  return getToken(PccIRParser::COLON, 0);
}

tree::TerminalNode* PccIRParser::TernaryExprContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

PccIRParser::TernaryExprContext::TernaryExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::TernaryExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitTernaryExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- ConvertExprContext ------------------------------------------------------------------

std::vector<tree::TerminalNode *> PccIRParser::ConvertExprContext::LPAREN() {
  return getTokens(PccIRParser::LPAREN);
}

tree::TerminalNode* PccIRParser::ConvertExprContext::LPAREN(size_t i) {
  return getToken(PccIRParser::LPAREN, i);
}

PccIRParser::TypeSpecContext* PccIRParser::ConvertExprContext::typeSpec() {
  return getRuleContext<PccIRParser::TypeSpecContext>(0);
}

std::vector<tree::TerminalNode *> PccIRParser::ConvertExprContext::RPAREN() {
  return getTokens(PccIRParser::RPAREN);
}

tree::TerminalNode* PccIRParser::ConvertExprContext::RPAREN(size_t i) {
  return getToken(PccIRParser::RPAREN, i);
}

PccIRParser::ExprContext* PccIRParser::ConvertExprContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

PccIRParser::ConvertExprContext::ConvertExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::ConvertExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitConvertExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CallExprContext ------------------------------------------------------------------

PccIRParser::RefContext* PccIRParser::CallExprContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

tree::TerminalNode* PccIRParser::CallExprContext::LPAREN() {
  return getToken(PccIRParser::LPAREN, 0);
}

tree::TerminalNode* PccIRParser::CallExprContext::RPAREN() {
  return getToken(PccIRParser::RPAREN, 0);
}

PccIRParser::ArgListContext* PccIRParser::CallExprContext::argList() {
  return getRuleContext<PccIRParser::ArgListContext>(0);
}

PccIRParser::CallExprContext::CallExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::CallExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitCallExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AssignExprContext ------------------------------------------------------------------

PccIRParser::RefContext* PccIRParser::AssignExprContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

tree::TerminalNode* PccIRParser::AssignExprContext::ASSIGN() {
  return getToken(PccIRParser::ASSIGN, 0);
}

PccIRParser::ExprContext* PccIRParser::AssignExprContext::expr() {
  return getRuleContext<PccIRParser::ExprContext>(0);
}

PccIRParser::AssignExprContext::AssignExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::AssignExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitAssignExpr(this);
  else
    return visitor->visitChildren(this);
}
//----------------- CompoundIndexExprContext ------------------------------------------------------------------

PccIRParser::RefContext* PccIRParser::CompoundIndexExprContext::ref() {
  return getRuleContext<PccIRParser::RefContext>(0);
}

tree::TerminalNode* PccIRParser::CompoundIndexExprContext::LBRACK() {
  return getToken(PccIRParser::LBRACK, 0);
}

std::vector<PccIRParser::ExprContext *> PccIRParser::CompoundIndexExprContext::expr() {
  return getRuleContexts<PccIRParser::ExprContext>();
}

PccIRParser::ExprContext* PccIRParser::CompoundIndexExprContext::expr(size_t i) {
  return getRuleContext<PccIRParser::ExprContext>(i);
}

tree::TerminalNode* PccIRParser::CompoundIndexExprContext::RBRACK() {
  return getToken(PccIRParser::RBRACK, 0);
}

PccIRParser::CompoundOpContext* PccIRParser::CompoundIndexExprContext::compoundOp() {
  return getRuleContext<PccIRParser::CompoundOpContext>(0);
}

tree::TerminalNode* PccIRParser::CompoundIndexExprContext::OLDVAL_KW() {
  return getToken(PccIRParser::OLDVAL_KW, 0);
}

PccIRParser::CompoundIndexExprContext::CompoundIndexExprContext(ExprContext *ctx) { copyFrom(ctx); }


std::any PccIRParser::CompoundIndexExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitCompoundIndexExpr(this);
  else
    return visitor->visitChildren(this);
}
PccIRParser::ExprContext* PccIRParser::expr() {
  ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, getState());
  enterRule(_localctx, 26, PccIRParser::RuleExpr);
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
      _localctx = _tracker.createInstance<PccIRParser::IntLitExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(210);
      match(PccIRParser::NUMBER);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PccIRParser::CompoundIndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(212);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::OLDVAL_KW) {
        setState(211);
        match(PccIRParser::OLDVAL_KW);
      }
      setState(214);
      ref();
      setState(215);
      match(PccIRParser::LBRACK);
      setState(216);
      expr();
      setState(217);
      match(PccIRParser::RBRACK);
      setState(218);
      compoundOp();
      setState(219);
      expr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<PccIRParser::CompoundAssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(222);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::OLDVAL_KW) {
        setState(221);
        match(PccIRParser::OLDVAL_KW);
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
      _localctx = _tracker.createInstance<PccIRParser::CompoundDerefExprContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(229);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::OLDVAL_KW) {
        setState(228);
        match(PccIRParser::OLDVAL_KW);
      }
      setState(231);
      match(PccIRParser::STAR);
      setState(232);
      match(PccIRParser::LPAREN);
      setState(233);
      expr();
      setState(234);
      match(PccIRParser::RPAREN);
      setState(235);
      compoundOp();
      setState(236);
      expr();
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<PccIRParser::IndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(238);
      ref();
      setState(239);
      match(PccIRParser::LBRACK);
      setState(240);
      expr();
      setState(241);
      match(PccIRParser::RBRACK);
      setState(244);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(242);
        match(PccIRParser::ASSIGN);
        setState(243);
        expr();
      }
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<PccIRParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(246);
      ref();
      setState(247);
      match(PccIRParser::ASSIGN);
      setState(248);
      expr();
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<PccIRParser::StarExprContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(250);
      match(PccIRParser::STAR);
      setState(251);
      match(PccIRParser::LPAREN);
      setState(252);
      expr();
      setState(253);
      match(PccIRParser::RPAREN);
      setState(256);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(254);
        match(PccIRParser::ASSIGN);
        setState(255);
        expr();
      }
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<PccIRParser::UnaryExprContext>(_localctx);
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
      match(PccIRParser::LPAREN);
      setState(260);
      expr();
      setState(261);
      match(PccIRParser::RPAREN);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<PccIRParser::BinaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(263);
      match(PccIRParser::LPAREN);
      setState(264);
      expr();
      setState(265);
      binOp();
      setState(266);
      expr();
      setState(267);
      match(PccIRParser::RPAREN);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<PccIRParser::TernaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(269);
      match(PccIRParser::LPAREN);
      setState(270);
      expr();
      setState(271);
      match(PccIRParser::QUESTION);
      setState(272);
      expr();
      setState(273);
      match(PccIRParser::COLON);
      setState(274);
      expr();
      setState(275);
      match(PccIRParser::RPAREN);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<PccIRParser::ConvertExprContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(277);
      match(PccIRParser::LPAREN);
      setState(278);
      typeSpec();
      setState(279);
      match(PccIRParser::RPAREN);
      setState(280);
      match(PccIRParser::LPAREN);
      setState(281);
      expr();
      setState(282);
      match(PccIRParser::RPAREN);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<PccIRParser::CallExprContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(284);
      ref();
      setState(285);
      match(PccIRParser::LPAREN);
      setState(287);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 6917529044284866560) != 0)) {
        setState(286);
        argList();
      }
      setState(289);
      match(PccIRParser::RPAREN);
      break;
    }

    case 13: {
      _localctx = _tracker.createInstance<PccIRParser::CallPtrExprContext>(_localctx);
      enterOuterAlt(_localctx, 13);
      setState(291);
      match(PccIRParser::LPAREN);
      setState(292);
      expr();
      setState(293);
      match(PccIRParser::RPAREN);
      setState(294);
      match(PccIRParser::LPAREN);
      setState(295);
      match(PccIRParser::RPAREN);
      break;
    }

    case 14: {
      _localctx = _tracker.createInstance<PccIRParser::IntrinsicExprContext>(_localctx);
      enterOuterAlt(_localctx, 14);
      setState(297);
      match(PccIRParser::IDENT);
      setState(298);
      match(PccIRParser::LPAREN);
      setState(299);
      match(PccIRParser::RPAREN);
      break;
    }

    case 15: {
      _localctx = _tracker.createInstance<PccIRParser::VarExprContext>(_localctx);
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

PccIRParser::ArgListContext::ArgListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<PccIRParser::ExprContext *> PccIRParser::ArgListContext::expr() {
  return getRuleContexts<PccIRParser::ExprContext>();
}

PccIRParser::ExprContext* PccIRParser::ArgListContext::expr(size_t i) {
  return getRuleContext<PccIRParser::ExprContext>(i);
}

std::vector<tree::TerminalNode *> PccIRParser::ArgListContext::COMMA() {
  return getTokens(PccIRParser::COMMA);
}

tree::TerminalNode* PccIRParser::ArgListContext::COMMA(size_t i) {
  return getToken(PccIRParser::COMMA, i);
}


size_t PccIRParser::ArgListContext::getRuleIndex() const {
  return PccIRParser::RuleArgList;
}


std::any PccIRParser::ArgListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitArgList(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::ArgListContext* PccIRParser::argList() {
  ArgListContext *_localctx = _tracker.createInstance<ArgListContext>(_ctx, getState());
  enterRule(_localctx, 28, PccIRParser::RuleArgList);
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
    while (_la == PccIRParser::COMMA) {
      setState(304);
      match(PccIRParser::COMMA);
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

PccIRParser::BinOpContext::BinOpContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccIRParser::BinOpContext::PLUS() {
  return getToken(PccIRParser::PLUS, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::MINUS() {
  return getToken(PccIRParser::MINUS, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::STAR() {
  return getToken(PccIRParser::STAR, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::SLASH() {
  return getToken(PccIRParser::SLASH, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::PERCENT() {
  return getToken(PccIRParser::PERCENT, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::SHL() {
  return getToken(PccIRParser::SHL, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::SHR() {
  return getToken(PccIRParser::SHR, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::EQ() {
  return getToken(PccIRParser::EQ, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::NE() {
  return getToken(PccIRParser::NE, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::LT() {
  return getToken(PccIRParser::LT, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::LE() {
  return getToken(PccIRParser::LE, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::GT() {
  return getToken(PccIRParser::GT, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::GE() {
  return getToken(PccIRParser::GE, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::AMP() {
  return getToken(PccIRParser::AMP, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::CARET() {
  return getToken(PccIRParser::CARET, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::PIPE() {
  return getToken(PccIRParser::PIPE, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::ANDAND() {
  return getToken(PccIRParser::ANDAND, 0);
}

tree::TerminalNode* PccIRParser::BinOpContext::OROR() {
  return getToken(PccIRParser::OROR, 0);
}


size_t PccIRParser::BinOpContext::getRuleIndex() const {
  return PccIRParser::RuleBinOp;
}


std::any PccIRParser::BinOpContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccIRParserVisitor*>(visitor))
    return parserVisitor->visitBinOp(this);
  else
    return visitor->visitChildren(this);
}

PccIRParser::BinOpContext* PccIRParser::binOp() {
  BinOpContext *_localctx = _tracker.createInstance<BinOpContext>(_ctx, getState());
  enterRule(_localctx, 30, PccIRParser::RuleBinOp);
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

void PccIRParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  pccirparserParserInitialize();
#else
  ::antlr4::internal::call_once(pccirparserParserOnceFlag, pccirparserParserInitialize);
#endif
}
