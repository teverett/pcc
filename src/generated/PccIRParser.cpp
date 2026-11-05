
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
      "paramList", "param", "block", "stmt", "ref", "expr", "argList", "binOp"
    },
    std::vector<std::string>{
      "", "'; --- globals ---'", "'; --- functions ---'", "'int'", "'char'", 
      "'short'", "'long'", "'unsigned'", "'local'", "'if'", "'else'", "'while'", 
      "'return'", "'break'", "'continue'", "'post'", "'('", "')'", "'['", 
      "']'", "'{'", "", "';'", "','", "'='", "'#'", "'*'", "'&'", "'-'", 
      "'!'", "'~'", "'/'", "'%'", "'+'", "'<<'", "'>>'", "'<='", "'>='", 
      "'<'", "'>'", "'=='", "'!='", "'^'", "'|'", "'&&'", "'||'"
    },
    std::vector<std::string>{
      "", "GLOBALS_HEADER", "FUNCTIONS_HEADER", "INT_KW", "CHAR_KW", "SHORT_KW", 
      "LONG_KW", "UNSIGNED_KW", "LOCAL_KW", "IF_KW", "ELSE_KW", "WHILE_KW", 
      "RETURN_KW", "BREAK_KW", "CONTINUE_KW", "POST_KW", "LPAREN", "RPAREN", 
      "LBRACK", "RBRACK", "LBRACE", "RBRACE", "SEMI", "COMMA", "ASSIGN", 
      "HASH", "STAR", "AMP", "MINUS", "BANG", "TILDE", "SLASH", "PERCENT", 
      "PLUS", "SHL", "SHR", "LE", "GE", "LT", "GT", "EQ", "NE", "CARET", 
      "PIPE", "ANDAND", "OROR", "IDENT", "NUMBER", "WS", "ASM_BEGIN", "ASM_TEXT", 
      "ASM_END"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,51,264,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,1,0,1,0,
  	5,0,31,8,0,10,0,12,0,34,9,0,1,0,1,0,5,0,38,8,0,10,0,12,0,41,9,0,1,0,1,
  	0,1,1,3,1,46,8,1,1,1,1,1,3,1,50,8,1,1,1,1,1,3,1,54,8,1,1,1,1,1,3,1,58,
  	8,1,1,1,1,1,1,1,3,1,63,8,1,1,1,3,1,66,8,1,1,1,5,1,69,8,1,10,1,12,1,72,
  	9,1,1,2,1,2,1,2,1,2,1,2,1,2,1,2,3,2,81,8,2,1,2,1,2,3,2,85,8,2,3,2,87,
  	8,2,1,2,1,2,1,3,1,3,1,3,1,3,5,3,95,8,3,10,3,12,3,98,9,3,3,3,100,8,3,1,
  	3,1,3,1,4,1,4,3,4,106,8,4,1,5,1,5,1,5,1,5,3,5,112,8,5,1,5,1,5,1,5,1,6,
  	1,6,1,6,5,6,120,8,6,10,6,12,6,123,9,6,1,7,1,7,1,7,1,8,1,8,5,8,130,8,8,
  	10,8,12,8,133,9,8,1,8,1,8,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,9,145,8,9,
  	1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,9,154,8,9,1,9,1,9,3,9,158,8,9,1,9,1,9,1,
  	9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,9,173,8,9,1,9,1,9,3,9,177,
  	8,9,3,9,179,8,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,9,190,8,9,1,10,
  	1,10,1,10,1,10,1,11,1,11,1,11,1,11,1,11,1,11,1,11,3,11,203,8,11,1,11,
  	1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,3,11,215,8,11,1,11,1,11,
  	1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,
  	1,11,1,11,1,11,1,11,1,11,3,11,238,8,11,1,11,1,11,1,11,1,11,1,11,1,11,
  	1,11,1,11,1,11,1,11,1,11,1,11,3,11,252,8,11,1,12,1,12,1,12,5,12,257,8,
  	12,10,12,12,12,260,9,12,1,13,1,13,1,13,0,0,14,0,2,4,6,8,10,12,14,16,18,
  	20,22,24,26,0,2,1,0,27,30,2,0,26,28,31,45,299,0,28,1,0,0,0,2,65,1,0,0,
  	0,4,73,1,0,0,0,6,90,1,0,0,0,8,105,1,0,0,0,10,107,1,0,0,0,12,116,1,0,0,
  	0,14,124,1,0,0,0,16,127,1,0,0,0,18,189,1,0,0,0,20,191,1,0,0,0,22,251,
  	1,0,0,0,24,253,1,0,0,0,26,261,1,0,0,0,28,32,5,1,0,0,29,31,3,4,2,0,30,
  	29,1,0,0,0,31,34,1,0,0,0,32,30,1,0,0,0,32,33,1,0,0,0,33,35,1,0,0,0,34,
  	32,1,0,0,0,35,39,5,2,0,0,36,38,3,10,5,0,37,36,1,0,0,0,38,41,1,0,0,0,39,
  	37,1,0,0,0,39,40,1,0,0,0,40,42,1,0,0,0,41,39,1,0,0,0,42,43,5,0,0,1,43,
  	1,1,0,0,0,44,46,5,7,0,0,45,44,1,0,0,0,45,46,1,0,0,0,46,47,1,0,0,0,47,
  	66,5,4,0,0,48,50,5,7,0,0,49,48,1,0,0,0,49,50,1,0,0,0,50,51,1,0,0,0,51,
  	66,5,5,0,0,52,54,5,7,0,0,53,52,1,0,0,0,53,54,1,0,0,0,54,55,1,0,0,0,55,
  	66,5,3,0,0,56,58,5,7,0,0,57,56,1,0,0,0,57,58,1,0,0,0,58,59,1,0,0,0,59,
  	60,5,6,0,0,60,66,5,6,0,0,61,63,5,7,0,0,62,61,1,0,0,0,62,63,1,0,0,0,63,
  	64,1,0,0,0,64,66,5,6,0,0,65,45,1,0,0,0,65,49,1,0,0,0,65,53,1,0,0,0,65,
  	57,1,0,0,0,65,62,1,0,0,0,66,70,1,0,0,0,67,69,5,26,0,0,68,67,1,0,0,0,69,
  	72,1,0,0,0,70,68,1,0,0,0,70,71,1,0,0,0,71,3,1,0,0,0,72,70,1,0,0,0,73,
  	74,3,2,1,0,74,86,3,20,10,0,75,76,5,18,0,0,76,77,5,47,0,0,77,80,5,19,0,
  	0,78,79,5,24,0,0,79,81,3,6,3,0,80,78,1,0,0,0,80,81,1,0,0,0,81,87,1,0,
  	0,0,82,83,5,24,0,0,83,85,5,47,0,0,84,82,1,0,0,0,84,85,1,0,0,0,85,87,1,
  	0,0,0,86,75,1,0,0,0,86,84,1,0,0,0,87,88,1,0,0,0,88,89,5,22,0,0,89,5,1,
  	0,0,0,90,99,5,20,0,0,91,96,3,8,4,0,92,93,5,23,0,0,93,95,3,8,4,0,94,92,
  	1,0,0,0,95,98,1,0,0,0,96,94,1,0,0,0,96,97,1,0,0,0,97,100,1,0,0,0,98,96,
  	1,0,0,0,99,91,1,0,0,0,99,100,1,0,0,0,100,101,1,0,0,0,101,102,5,21,0,0,
  	102,7,1,0,0,0,103,106,5,47,0,0,104,106,3,20,10,0,105,103,1,0,0,0,105,
  	104,1,0,0,0,106,9,1,0,0,0,107,108,3,2,1,0,108,109,3,20,10,0,109,111,5,
  	16,0,0,110,112,3,12,6,0,111,110,1,0,0,0,111,112,1,0,0,0,112,113,1,0,0,
  	0,113,114,5,17,0,0,114,115,3,16,8,0,115,11,1,0,0,0,116,121,3,14,7,0,117,
  	118,5,23,0,0,118,120,3,14,7,0,119,117,1,0,0,0,120,123,1,0,0,0,121,119,
  	1,0,0,0,121,122,1,0,0,0,122,13,1,0,0,0,123,121,1,0,0,0,124,125,3,2,1,
  	0,125,126,3,20,10,0,126,15,1,0,0,0,127,131,5,20,0,0,128,130,3,18,9,0,
  	129,128,1,0,0,0,130,133,1,0,0,0,131,129,1,0,0,0,131,132,1,0,0,0,132,134,
  	1,0,0,0,133,131,1,0,0,0,134,135,5,21,0,0,135,17,1,0,0,0,136,190,3,16,
  	8,0,137,138,5,9,0,0,138,139,5,16,0,0,139,140,3,22,11,0,140,141,5,17,0,
  	0,141,144,3,18,9,0,142,143,5,10,0,0,143,145,3,18,9,0,144,142,1,0,0,0,
  	144,145,1,0,0,0,145,190,1,0,0,0,146,147,5,11,0,0,147,148,5,16,0,0,148,
  	149,3,22,11,0,149,150,5,17,0,0,150,153,3,18,9,0,151,152,5,15,0,0,152,
  	154,3,18,9,0,153,151,1,0,0,0,153,154,1,0,0,0,154,190,1,0,0,0,155,157,
  	5,12,0,0,156,158,3,22,11,0,157,156,1,0,0,0,157,158,1,0,0,0,158,159,1,
  	0,0,0,159,190,5,22,0,0,160,161,5,13,0,0,161,190,5,22,0,0,162,163,5,14,
  	0,0,163,190,5,22,0,0,164,165,5,8,0,0,165,166,3,2,1,0,166,178,3,20,10,
  	0,167,168,5,18,0,0,168,169,5,47,0,0,169,172,5,19,0,0,170,171,5,24,0,0,
  	171,173,3,6,3,0,172,170,1,0,0,0,172,173,1,0,0,0,173,179,1,0,0,0,174,175,
  	5,24,0,0,175,177,3,22,11,0,176,174,1,0,0,0,176,177,1,0,0,0,177,179,1,
  	0,0,0,178,167,1,0,0,0,178,176,1,0,0,0,179,180,1,0,0,0,180,181,5,22,0,
  	0,181,190,1,0,0,0,182,183,5,49,0,0,183,184,5,50,0,0,184,190,5,51,0,0,
  	185,190,5,22,0,0,186,187,3,22,11,0,187,188,5,22,0,0,188,190,1,0,0,0,189,
  	136,1,0,0,0,189,137,1,0,0,0,189,146,1,0,0,0,189,155,1,0,0,0,189,160,1,
  	0,0,0,189,162,1,0,0,0,189,164,1,0,0,0,189,182,1,0,0,0,189,185,1,0,0,0,
  	189,186,1,0,0,0,190,19,1,0,0,0,191,192,5,46,0,0,192,193,5,25,0,0,193,
  	194,5,47,0,0,194,21,1,0,0,0,195,252,5,47,0,0,196,197,3,20,10,0,197,198,
  	5,18,0,0,198,199,3,22,11,0,199,202,5,19,0,0,200,201,5,24,0,0,201,203,
  	3,22,11,0,202,200,1,0,0,0,202,203,1,0,0,0,203,252,1,0,0,0,204,205,3,20,
  	10,0,205,206,5,24,0,0,206,207,3,22,11,0,207,252,1,0,0,0,208,209,5,26,
  	0,0,209,210,5,16,0,0,210,211,3,22,11,0,211,214,5,17,0,0,212,213,5,24,
  	0,0,213,215,3,22,11,0,214,212,1,0,0,0,214,215,1,0,0,0,215,252,1,0,0,0,
  	216,217,7,0,0,0,217,218,5,16,0,0,218,219,3,22,11,0,219,220,5,17,0,0,220,
  	252,1,0,0,0,221,222,5,16,0,0,222,223,3,22,11,0,223,224,3,26,13,0,224,
  	225,3,22,11,0,225,226,5,17,0,0,226,252,1,0,0,0,227,228,5,16,0,0,228,229,
  	3,2,1,0,229,230,5,17,0,0,230,231,5,16,0,0,231,232,3,22,11,0,232,233,5,
  	17,0,0,233,252,1,0,0,0,234,235,3,20,10,0,235,237,5,16,0,0,236,238,3,24,
  	12,0,237,236,1,0,0,0,237,238,1,0,0,0,238,239,1,0,0,0,239,240,5,17,0,0,
  	240,252,1,0,0,0,241,242,5,16,0,0,242,243,3,22,11,0,243,244,5,17,0,0,244,
  	245,5,16,0,0,245,246,5,17,0,0,246,252,1,0,0,0,247,248,5,46,0,0,248,249,
  	5,16,0,0,249,252,5,17,0,0,250,252,3,20,10,0,251,195,1,0,0,0,251,196,1,
  	0,0,0,251,204,1,0,0,0,251,208,1,0,0,0,251,216,1,0,0,0,251,221,1,0,0,0,
  	251,227,1,0,0,0,251,234,1,0,0,0,251,241,1,0,0,0,251,247,1,0,0,0,251,250,
  	1,0,0,0,252,23,1,0,0,0,253,258,3,22,11,0,254,255,5,23,0,0,255,257,3,22,
  	11,0,256,254,1,0,0,0,257,260,1,0,0,0,258,256,1,0,0,0,258,259,1,0,0,0,
  	259,25,1,0,0,0,260,258,1,0,0,0,261,262,7,1,0,0,262,27,1,0,0,0,30,32,39,
  	45,49,53,57,62,65,70,80,84,86,96,99,105,111,121,131,144,153,157,172,176,
  	178,189,202,214,237,251,258
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
    setState(28);
    match(PccIRParser::GLOBALS_HEADER);
    setState(32);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(29);
      globalDecl();
      setState(34);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(35);
    match(PccIRParser::FUNCTIONS_HEADER);
    setState(39);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(36);
      funcDecl();
      setState(41);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(42);
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
    setState(65);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 7, _ctx)) {
    case 1: {
      setState(45);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(44);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(47);
      match(PccIRParser::CHAR_KW);
      break;
    }

    case 2: {
      setState(49);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(48);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(51);
      match(PccIRParser::SHORT_KW);
      break;
    }

    case 3: {
      setState(53);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(52);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(55);
      match(PccIRParser::INT_KW);
      break;
    }

    case 4: {
      setState(57);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(56);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(59);
      match(PccIRParser::LONG_KW);
      setState(60);
      match(PccIRParser::LONG_KW);
      break;
    }

    case 5: {
      setState(62);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::UNSIGNED_KW) {
        setState(61);
        match(PccIRParser::UNSIGNED_KW);
      }
      setState(64);
      match(PccIRParser::LONG_KW);
      break;
    }

    default:
      break;
    }
    setState(70);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::STAR) {
      setState(67);
      match(PccIRParser::STAR);
      setState(72);
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
    setState(73);
    typeSpec();
    setState(74);
    ref();
    setState(86);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::LBRACK: {
        setState(75);
        match(PccIRParser::LBRACK);
        setState(76);
        match(PccIRParser::NUMBER);
        setState(77);
        match(PccIRParser::RBRACK);
        setState(80);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(78);
          match(PccIRParser::ASSIGN);
          setState(79);
          initList();
        }
        break;
      }

      case PccIRParser::SEMI:
      case PccIRParser::ASSIGN: {
        setState(84);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(82);
          match(PccIRParser::ASSIGN);
          setState(83);
          match(PccIRParser::NUMBER);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(88);
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
    setState(90);
    match(PccIRParser::LBRACE);
    setState(99);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == PccIRParser::IDENT

    || _la == PccIRParser::NUMBER) {
      setState(91);
      initElem();
      setState(96);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == PccIRParser::COMMA) {
        setState(92);
        match(PccIRParser::COMMA);
        setState(93);
        initElem();
        setState(98);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(101);
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
    setState(105);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::NUMBER: {
        enterOuterAlt(_localctx, 1);
        setState(103);
        match(PccIRParser::NUMBER);
        break;
      }

      case PccIRParser::IDENT: {
        enterOuterAlt(_localctx, 2);
        setState(104);
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
    setState(107);
    typeSpec();
    setState(108);
    ref();
    setState(109);
    match(PccIRParser::LPAREN);
    setState(111);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(110);
      paramList();
    }
    setState(113);
    match(PccIRParser::RPAREN);
    setState(114);
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
    setState(116);
    param();
    setState(121);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::COMMA) {
      setState(117);
      match(PccIRParser::COMMA);
      setState(118);
      param();
      setState(123);
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
    setState(124);
    typeSpec();
    setState(125);
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
    setState(127);
    match(PccIRParser::LBRACE);
    setState(131);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 774058271668992) != 0)) {
      setState(128);
      stmt();
      setState(133);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(134);
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
    setState(189);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::LBRACE: {
        _localctx = _tracker.createInstance<PccIRParser::BlockStmtContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(136);
        block();
        break;
      }

      case PccIRParser::IF_KW: {
        _localctx = _tracker.createInstance<PccIRParser::IfStmtContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(137);
        match(PccIRParser::IF_KW);
        setState(138);
        match(PccIRParser::LPAREN);
        setState(139);
        expr();
        setState(140);
        match(PccIRParser::RPAREN);
        setState(141);
        stmt();
        setState(144);
        _errHandler->sync(this);

        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 18, _ctx)) {
        case 1: {
          setState(142);
          match(PccIRParser::ELSE_KW);
          setState(143);
          stmt();
          break;
        }

        default:
          break;
        }
        break;
      }

      case PccIRParser::WHILE_KW: {
        _localctx = _tracker.createInstance<PccIRParser::WhileStmtContext>(_localctx);
        enterOuterAlt(_localctx, 3);
        setState(146);
        match(PccIRParser::WHILE_KW);
        setState(147);
        match(PccIRParser::LPAREN);
        setState(148);
        expr();
        setState(149);
        match(PccIRParser::RPAREN);
        setState(150);
        stmt();
        setState(153);
        _errHandler->sync(this);

        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 19, _ctx)) {
        case 1: {
          setState(151);
          match(PccIRParser::POST_KW);
          setState(152);
          stmt();
          break;
        }

        default:
          break;
        }
        break;
      }

      case PccIRParser::RETURN_KW: {
        _localctx = _tracker.createInstance<PccIRParser::ReturnStmtContext>(_localctx);
        enterOuterAlt(_localctx, 4);
        setState(155);
        match(PccIRParser::RETURN_KW);
        setState(157);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 211108312973312) != 0)) {
          setState(156);
          expr();
        }
        setState(159);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::BREAK_KW: {
        _localctx = _tracker.createInstance<PccIRParser::BreakStmtContext>(_localctx);
        enterOuterAlt(_localctx, 5);
        setState(160);
        match(PccIRParser::BREAK_KW);
        setState(161);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::CONTINUE_KW: {
        _localctx = _tracker.createInstance<PccIRParser::ContinueStmtContext>(_localctx);
        enterOuterAlt(_localctx, 6);
        setState(162);
        match(PccIRParser::CONTINUE_KW);
        setState(163);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::LOCAL_KW: {
        _localctx = _tracker.createInstance<PccIRParser::LocalDeclStmtContext>(_localctx);
        enterOuterAlt(_localctx, 7);
        setState(164);
        match(PccIRParser::LOCAL_KW);
        setState(165);
        typeSpec();
        setState(166);
        ref();
        setState(178);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case PccIRParser::LBRACK: {
            setState(167);
            match(PccIRParser::LBRACK);
            setState(168);
            match(PccIRParser::NUMBER);
            setState(169);
            match(PccIRParser::RBRACK);
            setState(172);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == PccIRParser::ASSIGN) {
              setState(170);
              match(PccIRParser::ASSIGN);
              setState(171);
              initList();
            }
            break;
          }

          case PccIRParser::SEMI:
          case PccIRParser::ASSIGN: {
            setState(176);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == PccIRParser::ASSIGN) {
              setState(174);
              match(PccIRParser::ASSIGN);
              setState(175);
              expr();
            }
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        setState(180);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::ASM_BEGIN: {
        _localctx = _tracker.createInstance<PccIRParser::AsmStmtContext>(_localctx);
        enterOuterAlt(_localctx, 8);
        setState(182);
        match(PccIRParser::ASM_BEGIN);
        setState(183);
        match(PccIRParser::ASM_TEXT);
        setState(184);
        match(PccIRParser::ASM_END);
        break;
      }

      case PccIRParser::SEMI: {
        _localctx = _tracker.createInstance<PccIRParser::EmptyStmtContext>(_localctx);
        enterOuterAlt(_localctx, 9);
        setState(185);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::LPAREN:
      case PccIRParser::STAR:
      case PccIRParser::AMP:
      case PccIRParser::MINUS:
      case PccIRParser::BANG:
      case PccIRParser::TILDE:
      case PccIRParser::IDENT:
      case PccIRParser::NUMBER: {
        _localctx = _tracker.createInstance<PccIRParser::ExprStmtContext>(_localctx);
        enterOuterAlt(_localctx, 10);
        setState(186);
        expr();
        setState(187);
        match(PccIRParser::SEMI);
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
    setState(191);
    match(PccIRParser::IDENT);
    setState(192);
    match(PccIRParser::HASH);
    setState(193);
    match(PccIRParser::NUMBER);
   
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
PccIRParser::ExprContext* PccIRParser::expr() {
  ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, getState());
  enterRule(_localctx, 22, PccIRParser::RuleExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(251);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 28, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<PccIRParser::IntLitExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(195);
      match(PccIRParser::NUMBER);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PccIRParser::IndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(196);
      ref();
      setState(197);
      match(PccIRParser::LBRACK);
      setState(198);
      expr();
      setState(199);
      match(PccIRParser::RBRACK);
      setState(202);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(200);
        match(PccIRParser::ASSIGN);
        setState(201);
        expr();
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<PccIRParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(204);
      ref();
      setState(205);
      match(PccIRParser::ASSIGN);
      setState(206);
      expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<PccIRParser::StarExprContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(208);
      match(PccIRParser::STAR);
      setState(209);
      match(PccIRParser::LPAREN);
      setState(210);
      expr();
      setState(211);
      match(PccIRParser::RPAREN);
      setState(214);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(212);
        match(PccIRParser::ASSIGN);
        setState(213);
        expr();
      }
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<PccIRParser::UnaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(216);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 2013265920) != 0))) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(217);
      match(PccIRParser::LPAREN);
      setState(218);
      expr();
      setState(219);
      match(PccIRParser::RPAREN);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<PccIRParser::BinaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(221);
      match(PccIRParser::LPAREN);
      setState(222);
      expr();
      setState(223);
      binOp();
      setState(224);
      expr();
      setState(225);
      match(PccIRParser::RPAREN);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<PccIRParser::ConvertExprContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(227);
      match(PccIRParser::LPAREN);
      setState(228);
      typeSpec();
      setState(229);
      match(PccIRParser::RPAREN);
      setState(230);
      match(PccIRParser::LPAREN);
      setState(231);
      expr();
      setState(232);
      match(PccIRParser::RPAREN);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<PccIRParser::CallExprContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(234);
      ref();
      setState(235);
      match(PccIRParser::LPAREN);
      setState(237);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 211108312973312) != 0)) {
        setState(236);
        argList();
      }
      setState(239);
      match(PccIRParser::RPAREN);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<PccIRParser::CallPtrExprContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(241);
      match(PccIRParser::LPAREN);
      setState(242);
      expr();
      setState(243);
      match(PccIRParser::RPAREN);
      setState(244);
      match(PccIRParser::LPAREN);
      setState(245);
      match(PccIRParser::RPAREN);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<PccIRParser::IntrinsicExprContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(247);
      match(PccIRParser::IDENT);
      setState(248);
      match(PccIRParser::LPAREN);
      setState(249);
      match(PccIRParser::RPAREN);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<PccIRParser::VarExprContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(250);
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
  enterRule(_localctx, 24, PccIRParser::RuleArgList);
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
    setState(253);
    expr();
    setState(258);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::COMMA) {
      setState(254);
      match(PccIRParser::COMMA);
      setState(255);
      expr();
      setState(260);
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
  enterRule(_localctx, 26, PccIRParser::RuleBinOp);
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
    setState(261);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 70367066456064) != 0))) {
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
