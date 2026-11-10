
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
      "paramList", "param", "block", "stmt", "ref", "compoundOp", "expr", 
      "argList", "binOp"
    },
    std::vector<std::string>{
      "", "'; --- globals ---'", "'; --- functions ---'", "'int'", "'char'", 
      "'short'", "'long'", "'unsigned'", "'local'", "'if'", "'else'", "'while'", 
      "'return'", "'break'", "'continue'", "'goto'", "'post'", "'oldval'", 
      "'('", "')'", "'['", "']'", "'{'", "", "';'", "','", "'='", "'#'", 
      "'*'", "'&'", "'-'", "'!'", "'~'", "'/'", "'%'", "'+'", "'<<'", "'>>'", 
      "'<='", "'>='", "'<'", "'>'", "'=='", "'!='", "'^'", "'|'", "'&&'", 
      "'||'", "'+='", "'-='", "'*='", "'/='", "'%='", "'&='", "'^='", "'|='", 
      "'<<='", "'>>='", "'\\u003F'", "':'"
    },
    std::vector<std::string>{
      "", "GLOBALS_HEADER", "FUNCTIONS_HEADER", "INT_KW", "CHAR_KW", "SHORT_KW", 
      "LONG_KW", "UNSIGNED_KW", "LOCAL_KW", "IF_KW", "ELSE_KW", "WHILE_KW", 
      "RETURN_KW", "BREAK_KW", "CONTINUE_KW", "GOTO_KW", "POST_KW", "OLDVAL_KW", 
      "LPAREN", "RPAREN", "LBRACK", "RBRACK", "LBRACE", "RBRACE", "SEMI", 
      "COMMA", "ASSIGN", "HASH", "STAR", "AMP", "MINUS", "BANG", "TILDE", 
      "SLASH", "PERCENT", "PLUS", "SHL", "SHR", "LE", "GE", "LT", "GT", 
      "EQ", "NE", "CARET", "PIPE", "ANDAND", "OROR", "PLUSEQ", "MINUSEQ", 
      "STAREQ", "SLASHEQ", "PERCENTEQ", "AMPEQ", "CARETEQ", "PIPEEQ", "SHLEQ", 
      "SHREQ", "QUESTION", "COLON", "IDENT", "NUMBER", "WS", "ASM_BEGIN", 
      "ASM_TEXT", "ASM_END"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,65,308,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,1,0,1,0,5,0,33,8,0,10,0,12,0,36,9,0,1,0,1,0,5,0,40,8,0,10,0,12,0,43,
  	9,0,1,0,1,0,1,1,3,1,48,8,1,1,1,1,1,3,1,52,8,1,1,1,1,1,3,1,56,8,1,1,1,
  	1,1,3,1,60,8,1,1,1,1,1,1,1,3,1,65,8,1,1,1,3,1,68,8,1,1,1,5,1,71,8,1,10,
  	1,12,1,74,9,1,1,2,1,2,1,2,1,2,1,2,1,2,1,2,3,2,83,8,2,1,2,1,2,3,2,87,8,
  	2,3,2,89,8,2,1,2,1,2,1,3,1,3,1,3,1,3,5,3,97,8,3,10,3,12,3,100,9,3,3,3,
  	102,8,3,1,3,1,3,1,4,1,4,3,4,108,8,4,1,5,1,5,1,5,1,5,3,5,114,8,5,1,5,1,
  	5,1,5,1,6,1,6,1,6,5,6,122,8,6,10,6,12,6,125,9,6,1,7,1,7,1,7,1,8,1,8,5,
  	8,132,8,8,10,8,12,8,135,9,8,1,8,1,8,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,
  	9,147,8,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,9,156,8,9,1,9,1,9,3,9,160,8,9,
  	1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,9,175,8,9,1,9,1,
  	9,3,9,179,8,9,3,9,181,8,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,
  	1,9,1,9,1,9,3,9,197,8,9,1,10,1,10,1,10,1,10,1,11,1,11,1,12,1,12,3,12,
  	207,8,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,3,12,217,8,12,1,12,1,
  	12,1,12,1,12,1,12,3,12,224,8,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,
  	12,1,12,1,12,1,12,1,12,1,12,3,12,239,8,12,1,12,1,12,1,12,1,12,1,12,1,
  	12,1,12,1,12,1,12,1,12,3,12,251,8,12,1,12,1,12,1,12,1,12,1,12,1,12,1,
  	12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,
  	12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,3,12,282,8,12,1,12,1,12,1,
  	12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,3,12,296,8,12,1,13,1,
  	13,1,13,5,13,301,8,13,10,13,12,13,304,9,13,1,14,1,14,1,14,0,0,15,0,2,
  	4,6,8,10,12,14,16,18,20,22,24,26,28,0,3,1,0,48,57,1,0,29,32,2,0,28,30,
  	33,47,351,0,30,1,0,0,0,2,67,1,0,0,0,4,75,1,0,0,0,6,92,1,0,0,0,8,107,1,
  	0,0,0,10,109,1,0,0,0,12,118,1,0,0,0,14,126,1,0,0,0,16,129,1,0,0,0,18,
  	196,1,0,0,0,20,198,1,0,0,0,22,202,1,0,0,0,24,295,1,0,0,0,26,297,1,0,0,
  	0,28,305,1,0,0,0,30,34,5,1,0,0,31,33,3,4,2,0,32,31,1,0,0,0,33,36,1,0,
  	0,0,34,32,1,0,0,0,34,35,1,0,0,0,35,37,1,0,0,0,36,34,1,0,0,0,37,41,5,2,
  	0,0,38,40,3,10,5,0,39,38,1,0,0,0,40,43,1,0,0,0,41,39,1,0,0,0,41,42,1,
  	0,0,0,42,44,1,0,0,0,43,41,1,0,0,0,44,45,5,0,0,1,45,1,1,0,0,0,46,48,5,
  	7,0,0,47,46,1,0,0,0,47,48,1,0,0,0,48,49,1,0,0,0,49,68,5,4,0,0,50,52,5,
  	7,0,0,51,50,1,0,0,0,51,52,1,0,0,0,52,53,1,0,0,0,53,68,5,5,0,0,54,56,5,
  	7,0,0,55,54,1,0,0,0,55,56,1,0,0,0,56,57,1,0,0,0,57,68,5,3,0,0,58,60,5,
  	7,0,0,59,58,1,0,0,0,59,60,1,0,0,0,60,61,1,0,0,0,61,62,5,6,0,0,62,68,5,
  	6,0,0,63,65,5,7,0,0,64,63,1,0,0,0,64,65,1,0,0,0,65,66,1,0,0,0,66,68,5,
  	6,0,0,67,47,1,0,0,0,67,51,1,0,0,0,67,55,1,0,0,0,67,59,1,0,0,0,67,64,1,
  	0,0,0,68,72,1,0,0,0,69,71,5,28,0,0,70,69,1,0,0,0,71,74,1,0,0,0,72,70,
  	1,0,0,0,72,73,1,0,0,0,73,3,1,0,0,0,74,72,1,0,0,0,75,76,3,2,1,0,76,88,
  	3,20,10,0,77,78,5,20,0,0,78,79,5,61,0,0,79,82,5,21,0,0,80,81,5,26,0,0,
  	81,83,3,6,3,0,82,80,1,0,0,0,82,83,1,0,0,0,83,89,1,0,0,0,84,85,5,26,0,
  	0,85,87,5,61,0,0,86,84,1,0,0,0,86,87,1,0,0,0,87,89,1,0,0,0,88,77,1,0,
  	0,0,88,86,1,0,0,0,89,90,1,0,0,0,90,91,5,24,0,0,91,5,1,0,0,0,92,101,5,
  	22,0,0,93,98,3,8,4,0,94,95,5,25,0,0,95,97,3,8,4,0,96,94,1,0,0,0,97,100,
  	1,0,0,0,98,96,1,0,0,0,98,99,1,0,0,0,99,102,1,0,0,0,100,98,1,0,0,0,101,
  	93,1,0,0,0,101,102,1,0,0,0,102,103,1,0,0,0,103,104,5,23,0,0,104,7,1,0,
  	0,0,105,108,5,61,0,0,106,108,3,20,10,0,107,105,1,0,0,0,107,106,1,0,0,
  	0,108,9,1,0,0,0,109,110,3,2,1,0,110,111,3,20,10,0,111,113,5,18,0,0,112,
  	114,3,12,6,0,113,112,1,0,0,0,113,114,1,0,0,0,114,115,1,0,0,0,115,116,
  	5,19,0,0,116,117,3,16,8,0,117,11,1,0,0,0,118,123,3,14,7,0,119,120,5,25,
  	0,0,120,122,3,14,7,0,121,119,1,0,0,0,122,125,1,0,0,0,123,121,1,0,0,0,
  	123,124,1,0,0,0,124,13,1,0,0,0,125,123,1,0,0,0,126,127,3,2,1,0,127,128,
  	3,20,10,0,128,15,1,0,0,0,129,133,5,22,0,0,130,132,3,18,9,0,131,130,1,
  	0,0,0,132,135,1,0,0,0,133,131,1,0,0,0,133,134,1,0,0,0,134,136,1,0,0,0,
  	135,133,1,0,0,0,136,137,5,23,0,0,137,17,1,0,0,0,138,197,3,16,8,0,139,
  	140,5,9,0,0,140,141,5,18,0,0,141,142,3,24,12,0,142,143,5,19,0,0,143,146,
  	3,18,9,0,144,145,5,10,0,0,145,147,3,18,9,0,146,144,1,0,0,0,146,147,1,
  	0,0,0,147,197,1,0,0,0,148,149,5,11,0,0,149,150,5,18,0,0,150,151,3,24,
  	12,0,151,152,5,19,0,0,152,155,3,18,9,0,153,154,5,16,0,0,154,156,3,18,
  	9,0,155,153,1,0,0,0,155,156,1,0,0,0,156,197,1,0,0,0,157,159,5,12,0,0,
  	158,160,3,24,12,0,159,158,1,0,0,0,159,160,1,0,0,0,160,161,1,0,0,0,161,
  	197,5,24,0,0,162,163,5,13,0,0,163,197,5,24,0,0,164,165,5,14,0,0,165,197,
  	5,24,0,0,166,167,5,8,0,0,167,168,3,2,1,0,168,180,3,20,10,0,169,170,5,
  	20,0,0,170,171,5,61,0,0,171,174,5,21,0,0,172,173,5,26,0,0,173,175,3,6,
  	3,0,174,172,1,0,0,0,174,175,1,0,0,0,175,181,1,0,0,0,176,177,5,26,0,0,
  	177,179,3,24,12,0,178,176,1,0,0,0,178,179,1,0,0,0,179,181,1,0,0,0,180,
  	169,1,0,0,0,180,178,1,0,0,0,181,182,1,0,0,0,182,183,5,24,0,0,183,197,
  	1,0,0,0,184,185,5,63,0,0,185,186,5,64,0,0,186,197,5,65,0,0,187,197,5,
  	24,0,0,188,189,5,60,0,0,189,197,5,59,0,0,190,191,5,15,0,0,191,192,5,60,
  	0,0,192,197,5,24,0,0,193,194,3,24,12,0,194,195,5,24,0,0,195,197,1,0,0,
  	0,196,138,1,0,0,0,196,139,1,0,0,0,196,148,1,0,0,0,196,157,1,0,0,0,196,
  	162,1,0,0,0,196,164,1,0,0,0,196,166,1,0,0,0,196,184,1,0,0,0,196,187,1,
  	0,0,0,196,188,1,0,0,0,196,190,1,0,0,0,196,193,1,0,0,0,197,19,1,0,0,0,
  	198,199,5,60,0,0,199,200,5,27,0,0,200,201,5,61,0,0,201,21,1,0,0,0,202,
  	203,7,0,0,0,203,23,1,0,0,0,204,296,5,61,0,0,205,207,5,17,0,0,206,205,
  	1,0,0,0,206,207,1,0,0,0,207,208,1,0,0,0,208,209,3,20,10,0,209,210,5,20,
  	0,0,210,211,3,24,12,0,211,212,5,21,0,0,212,213,3,22,11,0,213,214,3,24,
  	12,0,214,296,1,0,0,0,215,217,5,17,0,0,216,215,1,0,0,0,216,217,1,0,0,0,
  	217,218,1,0,0,0,218,219,3,20,10,0,219,220,3,22,11,0,220,221,3,24,12,0,
  	221,296,1,0,0,0,222,224,5,17,0,0,223,222,1,0,0,0,223,224,1,0,0,0,224,
  	225,1,0,0,0,225,226,5,28,0,0,226,227,5,18,0,0,227,228,3,24,12,0,228,229,
  	5,19,0,0,229,230,3,22,11,0,230,231,3,24,12,0,231,296,1,0,0,0,232,233,
  	3,20,10,0,233,234,5,20,0,0,234,235,3,24,12,0,235,238,5,21,0,0,236,237,
  	5,26,0,0,237,239,3,24,12,0,238,236,1,0,0,0,238,239,1,0,0,0,239,296,1,
  	0,0,0,240,241,3,20,10,0,241,242,5,26,0,0,242,243,3,24,12,0,243,296,1,
  	0,0,0,244,245,5,28,0,0,245,246,5,18,0,0,246,247,3,24,12,0,247,250,5,19,
  	0,0,248,249,5,26,0,0,249,251,3,24,12,0,250,248,1,0,0,0,250,251,1,0,0,
  	0,251,296,1,0,0,0,252,253,7,1,0,0,253,254,5,18,0,0,254,255,3,24,12,0,
  	255,256,5,19,0,0,256,296,1,0,0,0,257,258,5,18,0,0,258,259,3,24,12,0,259,
  	260,3,28,14,0,260,261,3,24,12,0,261,262,5,19,0,0,262,296,1,0,0,0,263,
  	264,5,18,0,0,264,265,3,24,12,0,265,266,5,58,0,0,266,267,3,24,12,0,267,
  	268,5,59,0,0,268,269,3,24,12,0,269,270,5,19,0,0,270,296,1,0,0,0,271,272,
  	5,18,0,0,272,273,3,2,1,0,273,274,5,19,0,0,274,275,5,18,0,0,275,276,3,
  	24,12,0,276,277,5,19,0,0,277,296,1,0,0,0,278,279,3,20,10,0,279,281,5,
  	18,0,0,280,282,3,26,13,0,281,280,1,0,0,0,281,282,1,0,0,0,282,283,1,0,
  	0,0,283,284,5,19,0,0,284,296,1,0,0,0,285,286,5,18,0,0,286,287,3,24,12,
  	0,287,288,5,19,0,0,288,289,5,18,0,0,289,290,5,19,0,0,290,296,1,0,0,0,
  	291,292,5,60,0,0,292,293,5,18,0,0,293,296,5,19,0,0,294,296,3,20,10,0,
  	295,204,1,0,0,0,295,206,1,0,0,0,295,216,1,0,0,0,295,223,1,0,0,0,295,232,
  	1,0,0,0,295,240,1,0,0,0,295,244,1,0,0,0,295,252,1,0,0,0,295,257,1,0,0,
  	0,295,263,1,0,0,0,295,271,1,0,0,0,295,278,1,0,0,0,295,285,1,0,0,0,295,
  	291,1,0,0,0,295,294,1,0,0,0,296,25,1,0,0,0,297,302,3,24,12,0,298,299,
  	5,25,0,0,299,301,3,24,12,0,300,298,1,0,0,0,301,304,1,0,0,0,302,300,1,
  	0,0,0,302,303,1,0,0,0,303,27,1,0,0,0,304,302,1,0,0,0,305,306,7,2,0,0,
  	306,29,1,0,0,0,33,34,41,47,51,55,59,64,67,72,82,86,88,98,101,107,113,
  	123,133,146,155,159,174,178,180,196,206,216,223,238,250,281,295,302
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
    setState(30);
    match(PccIRParser::GLOBALS_HEADER);
    setState(34);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(31);
      globalDecl();
      setState(36);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(37);
    match(PccIRParser::FUNCTIONS_HEADER);
    setState(41);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(38);
      funcDecl();
      setState(43);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(44);
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
    setState(67);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 7, _ctx)) {
    case 1: {
      setState(47);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(46);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(49);
      match(PccIRParser::CHAR_KW);
      break;
    }

    case 2: {
      setState(51);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(50);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(53);
      match(PccIRParser::SHORT_KW);
      break;
    }

    case 3: {
      setState(55);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(54);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(57);
      match(PccIRParser::INT_KW);
      break;
    }

    case 4: {
      setState(59);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(58);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(61);
      match(PccIRParser::LONG_KW);
      setState(62);
      match(PccIRParser::LONG_KW);
      break;
    }

    case 5: {
      setState(64);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(63);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(66);
      match(PccIRParser::LONG_KW);
      break;
    }

    default:
      break;
    }
    setState(72);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::STAR) {
      setState(69);
      match(PccIRParser::STAR);
      setState(74);
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
    setState(75);
    typeSpec();
    setState(76);
    ref();
    setState(88);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::LBRACK: {
        setState(77);
        match(PccIRParser::LBRACK);
        setState(78);
        match(PccIRParser::NUMBER);
        setState(79);
        match(PccIRParser::RBRACK);
        setState(82);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(80);
          match(PccIRParser::ASSIGN);
          setState(81);
          initList();
        }
        break;
      }

      case PccIRParser::SEMI:
      case PccIRParser::ASSIGN: {
        setState(86);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(84);
          match(PccIRParser::ASSIGN);
          setState(85);
          match(PccIRParser::NUMBER);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(90);
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
    setState(92);
    match(PccIRParser::LBRACE);
    setState(101);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == PccIRParser::IDENT

    || _la == PccIRParser::NUMBER) {
      setState(93);
      initElem();
      setState(98);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == PccIRParser::COMMA) {
        setState(94);
        match(PccIRParser::COMMA);
        setState(95);
        initElem();
        setState(100);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(103);
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
    setState(107);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::NUMBER: {
        enterOuterAlt(_localctx, 1);
        setState(105);
        match(PccIRParser::NUMBER);
        break;
      }

      case PccIRParser::IDENT: {
        enterOuterAlt(_localctx, 2);
        setState(106);
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

PccIRParser::TypeSpecContext* PccIRParser::FuncDeclContext::typeSpec() {
  return getRuleContext<PccIRParser::TypeSpecContext>(0);
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
    setState(109);
    typeSpec();
    setState(110);
    ref();
    setState(111);
    match(PccIRParser::LPAREN);
    setState(113);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(112);
      paramList();
    }
    setState(115);
    match(PccIRParser::RPAREN);
    setState(116);
    block();
   
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
  enterRule(_localctx, 12, PccIRParser::RuleParamList);
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
    setState(118);
    param();
    setState(123);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::COMMA) {
      setState(119);
      match(PccIRParser::COMMA);
      setState(120);
      param();
      setState(125);
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
  enterRule(_localctx, 14, PccIRParser::RuleParam);

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
    typeSpec();
    setState(127);
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
  enterRule(_localctx, 16, PccIRParser::RuleBlock);
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
    setState(129);
    match(PccIRParser::LBRACE);
    setState(133);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -5764607514691306752) != 0)) {
      setState(130);
      stmt();
      setState(135);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(136);
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
  enterRule(_localctx, 18, PccIRParser::RuleStmt);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(196);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 24, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<PccIRParser::BlockStmtContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(138);
      block();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PccIRParser::IfStmtContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(139);
      match(PccIRParser::IF_KW);
      setState(140);
      match(PccIRParser::LPAREN);
      setState(141);
      expr();
      setState(142);
      match(PccIRParser::RPAREN);
      setState(143);
      stmt();
      setState(146);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 18, _ctx)) {
      case 1: {
        setState(144);
        match(PccIRParser::ELSE_KW);
        setState(145);
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
      setState(148);
      match(PccIRParser::WHILE_KW);
      setState(149);
      match(PccIRParser::LPAREN);
      setState(150);
      expr();
      setState(151);
      match(PccIRParser::RPAREN);
      setState(152);
      stmt();
      setState(155);
      _errHandler->sync(this);

      switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 19, _ctx)) {
      case 1: {
        setState(153);
        match(PccIRParser::POST_KW);
        setState(154);
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
      setState(157);
      match(PccIRParser::RETURN_KW);
      setState(159);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 3458764522142433280) != 0)) {
        setState(158);
        expr();
      }
      setState(161);
      match(PccIRParser::SEMI);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<PccIRParser::BreakStmtContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(162);
      match(PccIRParser::BREAK_KW);
      setState(163);
      match(PccIRParser::SEMI);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<PccIRParser::ContinueStmtContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(164);
      match(PccIRParser::CONTINUE_KW);
      setState(165);
      match(PccIRParser::SEMI);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<PccIRParser::LocalDeclStmtContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(166);
      match(PccIRParser::LOCAL_KW);
      setState(167);
      typeSpec();
      setState(168);
      ref();
      setState(180);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case PccIRParser::LBRACK: {
          setState(169);
          match(PccIRParser::LBRACK);
          setState(170);
          match(PccIRParser::NUMBER);
          setState(171);
          match(PccIRParser::RBRACK);
          setState(174);
          _errHandler->sync(this);

          _la = _input->LA(1);
          if (_la == PccIRParser::ASSIGN) {
            setState(172);
            match(PccIRParser::ASSIGN);
            setState(173);
            initList();
          }
          break;
        }

        case PccIRParser::SEMI:
        case PccIRParser::ASSIGN: {
          setState(178);
          _errHandler->sync(this);

          _la = _input->LA(1);
          if (_la == PccIRParser::ASSIGN) {
            setState(176);
            match(PccIRParser::ASSIGN);
            setState(177);
            expr();
          }
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(182);
      match(PccIRParser::SEMI);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<PccIRParser::AsmStmtContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(184);
      match(PccIRParser::ASM_BEGIN);
      setState(185);
      match(PccIRParser::ASM_TEXT);
      setState(186);
      match(PccIRParser::ASM_END);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<PccIRParser::EmptyStmtContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(187);
      match(PccIRParser::SEMI);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<PccIRParser::LabelStmtContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(188);
      match(PccIRParser::IDENT);
      setState(189);
      match(PccIRParser::COLON);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<PccIRParser::GotoStmtContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(190);
      match(PccIRParser::GOTO_KW);
      setState(191);
      match(PccIRParser::IDENT);
      setState(192);
      match(PccIRParser::SEMI);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<PccIRParser::ExprStmtContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(193);
      expr();
      setState(194);
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
  enterRule(_localctx, 20, PccIRParser::RuleRef);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(198);
    match(PccIRParser::IDENT);
    setState(199);
    match(PccIRParser::HASH);
    setState(200);
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
  enterRule(_localctx, 22, PccIRParser::RuleCompoundOp);
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
    setState(202);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 287948901175001088) != 0))) {
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
  enterRule(_localctx, 24, PccIRParser::RuleExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(295);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 31, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<PccIRParser::IntLitExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(204);
      match(PccIRParser::NUMBER);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PccIRParser::CompoundIndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(206);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::OLDVAL_KW) {
        setState(205);
        match(PccIRParser::OLDVAL_KW);
      }
      setState(208);
      ref();
      setState(209);
      match(PccIRParser::LBRACK);
      setState(210);
      expr();
      setState(211);
      match(PccIRParser::RBRACK);
      setState(212);
      compoundOp();
      setState(213);
      expr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<PccIRParser::CompoundAssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(216);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::OLDVAL_KW) {
        setState(215);
        match(PccIRParser::OLDVAL_KW);
      }
      setState(218);
      ref();
      setState(219);
      compoundOp();
      setState(220);
      expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<PccIRParser::CompoundDerefExprContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(223);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::OLDVAL_KW) {
        setState(222);
        match(PccIRParser::OLDVAL_KW);
      }
      setState(225);
      match(PccIRParser::STAR);
      setState(226);
      match(PccIRParser::LPAREN);
      setState(227);
      expr();
      setState(228);
      match(PccIRParser::RPAREN);
      setState(229);
      compoundOp();
      setState(230);
      expr();
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<PccIRParser::IndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(232);
      ref();
      setState(233);
      match(PccIRParser::LBRACK);
      setState(234);
      expr();
      setState(235);
      match(PccIRParser::RBRACK);
      setState(238);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(236);
        match(PccIRParser::ASSIGN);
        setState(237);
        expr();
      }
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<PccIRParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(240);
      ref();
      setState(241);
      match(PccIRParser::ASSIGN);
      setState(242);
      expr();
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<PccIRParser::StarExprContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(244);
      match(PccIRParser::STAR);
      setState(245);
      match(PccIRParser::LPAREN);
      setState(246);
      expr();
      setState(247);
      match(PccIRParser::RPAREN);
      setState(250);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(248);
        match(PccIRParser::ASSIGN);
        setState(249);
        expr();
      }
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<PccIRParser::UnaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(252);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 8053063680) != 0))) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(253);
      match(PccIRParser::LPAREN);
      setState(254);
      expr();
      setState(255);
      match(PccIRParser::RPAREN);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<PccIRParser::BinaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(257);
      match(PccIRParser::LPAREN);
      setState(258);
      expr();
      setState(259);
      binOp();
      setState(260);
      expr();
      setState(261);
      match(PccIRParser::RPAREN);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<PccIRParser::TernaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(263);
      match(PccIRParser::LPAREN);
      setState(264);
      expr();
      setState(265);
      match(PccIRParser::QUESTION);
      setState(266);
      expr();
      setState(267);
      match(PccIRParser::COLON);
      setState(268);
      expr();
      setState(269);
      match(PccIRParser::RPAREN);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<PccIRParser::ConvertExprContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(271);
      match(PccIRParser::LPAREN);
      setState(272);
      typeSpec();
      setState(273);
      match(PccIRParser::RPAREN);
      setState(274);
      match(PccIRParser::LPAREN);
      setState(275);
      expr();
      setState(276);
      match(PccIRParser::RPAREN);
      break;
    }

    case 12: {
      _localctx = _tracker.createInstance<PccIRParser::CallExprContext>(_localctx);
      enterOuterAlt(_localctx, 12);
      setState(278);
      ref();
      setState(279);
      match(PccIRParser::LPAREN);
      setState(281);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 3458764522142433280) != 0)) {
        setState(280);
        argList();
      }
      setState(283);
      match(PccIRParser::RPAREN);
      break;
    }

    case 13: {
      _localctx = _tracker.createInstance<PccIRParser::CallPtrExprContext>(_localctx);
      enterOuterAlt(_localctx, 13);
      setState(285);
      match(PccIRParser::LPAREN);
      setState(286);
      expr();
      setState(287);
      match(PccIRParser::RPAREN);
      setState(288);
      match(PccIRParser::LPAREN);
      setState(289);
      match(PccIRParser::RPAREN);
      break;
    }

    case 14: {
      _localctx = _tracker.createInstance<PccIRParser::IntrinsicExprContext>(_localctx);
      enterOuterAlt(_localctx, 14);
      setState(291);
      match(PccIRParser::IDENT);
      setState(292);
      match(PccIRParser::LPAREN);
      setState(293);
      match(PccIRParser::RPAREN);
      break;
    }

    case 15: {
      _localctx = _tracker.createInstance<PccIRParser::VarExprContext>(_localctx);
      enterOuterAlt(_localctx, 15);
      setState(294);
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
  enterRule(_localctx, 26, PccIRParser::RuleArgList);
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
    setState(297);
    expr();
    setState(302);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::COMMA) {
      setState(298);
      match(PccIRParser::COMMA);
      setState(299);
      expr();
      setState(304);
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
  enterRule(_localctx, 28, PccIRParser::RuleBinOp);
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
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 281468265824256) != 0))) {
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
