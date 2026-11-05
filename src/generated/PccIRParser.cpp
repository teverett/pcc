
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
  	4,1,51,258,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,1,0,1,0,
  	5,0,31,8,0,10,0,12,0,34,9,0,1,0,1,0,5,0,38,8,0,10,0,12,0,41,9,0,1,0,1,
  	0,1,1,3,1,46,8,1,1,1,1,1,3,1,50,8,1,1,1,1,1,3,1,54,8,1,1,1,1,1,3,1,58,
  	8,1,1,1,1,1,1,1,3,1,63,8,1,1,1,3,1,66,8,1,1,2,1,2,1,2,1,2,1,2,1,2,1,2,
  	3,2,75,8,2,1,2,1,2,3,2,79,8,2,3,2,81,8,2,1,2,1,2,1,3,1,3,1,3,1,3,5,3,
  	89,8,3,10,3,12,3,92,9,3,3,3,94,8,3,1,3,1,3,1,4,1,4,3,4,100,8,4,1,5,1,
  	5,1,5,1,5,3,5,106,8,5,1,5,1,5,1,5,1,6,1,6,1,6,5,6,114,8,6,10,6,12,6,117,
  	9,6,1,7,1,7,1,7,1,8,1,8,5,8,124,8,8,10,8,12,8,127,9,8,1,8,1,8,1,9,1,9,
  	1,9,1,9,1,9,1,9,1,9,1,9,3,9,139,8,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,9,148,
  	8,9,1,9,1,9,3,9,152,8,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,
  	9,1,9,3,9,167,8,9,1,9,1,9,3,9,171,8,9,3,9,173,8,9,1,9,1,9,1,9,1,9,1,9,
  	1,9,1,9,1,9,1,9,3,9,184,8,9,1,10,1,10,1,10,1,10,1,11,1,11,1,11,1,11,1,
  	11,1,11,1,11,3,11,197,8,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,
  	11,1,11,3,11,209,8,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,
  	11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,3,11,232,8,
  	11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,3,11,246,
  	8,11,1,12,1,12,1,12,5,12,251,8,12,10,12,12,12,254,9,12,1,13,1,13,1,13,
  	0,0,14,0,2,4,6,8,10,12,14,16,18,20,22,24,26,0,2,1,0,27,30,2,0,26,28,31,
  	45,292,0,28,1,0,0,0,2,65,1,0,0,0,4,67,1,0,0,0,6,84,1,0,0,0,8,99,1,0,0,
  	0,10,101,1,0,0,0,12,110,1,0,0,0,14,118,1,0,0,0,16,121,1,0,0,0,18,183,
  	1,0,0,0,20,185,1,0,0,0,22,245,1,0,0,0,24,247,1,0,0,0,26,255,1,0,0,0,28,
  	32,5,1,0,0,29,31,3,4,2,0,30,29,1,0,0,0,31,34,1,0,0,0,32,30,1,0,0,0,32,
  	33,1,0,0,0,33,35,1,0,0,0,34,32,1,0,0,0,35,39,5,2,0,0,36,38,3,10,5,0,37,
  	36,1,0,0,0,38,41,1,0,0,0,39,37,1,0,0,0,39,40,1,0,0,0,40,42,1,0,0,0,41,
  	39,1,0,0,0,42,43,5,0,0,1,43,1,1,0,0,0,44,46,5,7,0,0,45,44,1,0,0,0,45,
  	46,1,0,0,0,46,47,1,0,0,0,47,66,5,4,0,0,48,50,5,7,0,0,49,48,1,0,0,0,49,
  	50,1,0,0,0,50,51,1,0,0,0,51,66,5,5,0,0,52,54,5,7,0,0,53,52,1,0,0,0,53,
  	54,1,0,0,0,54,55,1,0,0,0,55,66,5,3,0,0,56,58,5,7,0,0,57,56,1,0,0,0,57,
  	58,1,0,0,0,58,59,1,0,0,0,59,60,5,6,0,0,60,66,5,6,0,0,61,63,5,7,0,0,62,
  	61,1,0,0,0,62,63,1,0,0,0,63,64,1,0,0,0,64,66,5,6,0,0,65,45,1,0,0,0,65,
  	49,1,0,0,0,65,53,1,0,0,0,65,57,1,0,0,0,65,62,1,0,0,0,66,3,1,0,0,0,67,
  	68,3,2,1,0,68,80,3,20,10,0,69,70,5,18,0,0,70,71,5,47,0,0,71,74,5,19,0,
  	0,72,73,5,24,0,0,73,75,3,6,3,0,74,72,1,0,0,0,74,75,1,0,0,0,75,81,1,0,
  	0,0,76,77,5,24,0,0,77,79,5,47,0,0,78,76,1,0,0,0,78,79,1,0,0,0,79,81,1,
  	0,0,0,80,69,1,0,0,0,80,78,1,0,0,0,81,82,1,0,0,0,82,83,5,22,0,0,83,5,1,
  	0,0,0,84,93,5,20,0,0,85,90,3,8,4,0,86,87,5,23,0,0,87,89,3,8,4,0,88,86,
  	1,0,0,0,89,92,1,0,0,0,90,88,1,0,0,0,90,91,1,0,0,0,91,94,1,0,0,0,92,90,
  	1,0,0,0,93,85,1,0,0,0,93,94,1,0,0,0,94,95,1,0,0,0,95,96,5,21,0,0,96,7,
  	1,0,0,0,97,100,5,47,0,0,98,100,3,20,10,0,99,97,1,0,0,0,99,98,1,0,0,0,
  	100,9,1,0,0,0,101,102,3,2,1,0,102,103,3,20,10,0,103,105,5,16,0,0,104,
  	106,3,12,6,0,105,104,1,0,0,0,105,106,1,0,0,0,106,107,1,0,0,0,107,108,
  	5,17,0,0,108,109,3,16,8,0,109,11,1,0,0,0,110,115,3,14,7,0,111,112,5,23,
  	0,0,112,114,3,14,7,0,113,111,1,0,0,0,114,117,1,0,0,0,115,113,1,0,0,0,
  	115,116,1,0,0,0,116,13,1,0,0,0,117,115,1,0,0,0,118,119,3,2,1,0,119,120,
  	3,20,10,0,120,15,1,0,0,0,121,125,5,20,0,0,122,124,3,18,9,0,123,122,1,
  	0,0,0,124,127,1,0,0,0,125,123,1,0,0,0,125,126,1,0,0,0,126,128,1,0,0,0,
  	127,125,1,0,0,0,128,129,5,21,0,0,129,17,1,0,0,0,130,184,3,16,8,0,131,
  	132,5,9,0,0,132,133,5,16,0,0,133,134,3,22,11,0,134,135,5,17,0,0,135,138,
  	3,18,9,0,136,137,5,10,0,0,137,139,3,18,9,0,138,136,1,0,0,0,138,139,1,
  	0,0,0,139,184,1,0,0,0,140,141,5,11,0,0,141,142,5,16,0,0,142,143,3,22,
  	11,0,143,144,5,17,0,0,144,147,3,18,9,0,145,146,5,15,0,0,146,148,3,18,
  	9,0,147,145,1,0,0,0,147,148,1,0,0,0,148,184,1,0,0,0,149,151,5,12,0,0,
  	150,152,3,22,11,0,151,150,1,0,0,0,151,152,1,0,0,0,152,153,1,0,0,0,153,
  	184,5,22,0,0,154,155,5,13,0,0,155,184,5,22,0,0,156,157,5,14,0,0,157,184,
  	5,22,0,0,158,159,5,8,0,0,159,160,3,2,1,0,160,172,3,20,10,0,161,162,5,
  	18,0,0,162,163,5,47,0,0,163,166,5,19,0,0,164,165,5,24,0,0,165,167,3,6,
  	3,0,166,164,1,0,0,0,166,167,1,0,0,0,167,173,1,0,0,0,168,169,5,24,0,0,
  	169,171,3,22,11,0,170,168,1,0,0,0,170,171,1,0,0,0,171,173,1,0,0,0,172,
  	161,1,0,0,0,172,170,1,0,0,0,173,174,1,0,0,0,174,175,5,22,0,0,175,184,
  	1,0,0,0,176,177,5,49,0,0,177,178,5,50,0,0,178,184,5,51,0,0,179,184,5,
  	22,0,0,180,181,3,22,11,0,181,182,5,22,0,0,182,184,1,0,0,0,183,130,1,0,
  	0,0,183,131,1,0,0,0,183,140,1,0,0,0,183,149,1,0,0,0,183,154,1,0,0,0,183,
  	156,1,0,0,0,183,158,1,0,0,0,183,176,1,0,0,0,183,179,1,0,0,0,183,180,1,
  	0,0,0,184,19,1,0,0,0,185,186,5,46,0,0,186,187,5,25,0,0,187,188,5,47,0,
  	0,188,21,1,0,0,0,189,246,5,47,0,0,190,191,3,20,10,0,191,192,5,18,0,0,
  	192,193,3,22,11,0,193,196,5,19,0,0,194,195,5,24,0,0,195,197,3,22,11,0,
  	196,194,1,0,0,0,196,197,1,0,0,0,197,246,1,0,0,0,198,199,3,20,10,0,199,
  	200,5,24,0,0,200,201,3,22,11,0,201,246,1,0,0,0,202,203,5,26,0,0,203,204,
  	5,16,0,0,204,205,3,22,11,0,205,208,5,17,0,0,206,207,5,24,0,0,207,209,
  	3,22,11,0,208,206,1,0,0,0,208,209,1,0,0,0,209,246,1,0,0,0,210,211,7,0,
  	0,0,211,212,5,16,0,0,212,213,3,22,11,0,213,214,5,17,0,0,214,246,1,0,0,
  	0,215,216,5,16,0,0,216,217,3,22,11,0,217,218,3,26,13,0,218,219,3,22,11,
  	0,219,220,5,17,0,0,220,246,1,0,0,0,221,222,5,16,0,0,222,223,3,2,1,0,223,
  	224,5,17,0,0,224,225,5,16,0,0,225,226,3,22,11,0,226,227,5,17,0,0,227,
  	246,1,0,0,0,228,229,3,20,10,0,229,231,5,16,0,0,230,232,3,24,12,0,231,
  	230,1,0,0,0,231,232,1,0,0,0,232,233,1,0,0,0,233,234,5,17,0,0,234,246,
  	1,0,0,0,235,236,5,16,0,0,236,237,3,22,11,0,237,238,5,17,0,0,238,239,5,
  	16,0,0,239,240,5,17,0,0,240,246,1,0,0,0,241,242,5,46,0,0,242,243,5,16,
  	0,0,243,246,5,17,0,0,244,246,3,20,10,0,245,189,1,0,0,0,245,190,1,0,0,
  	0,245,198,1,0,0,0,245,202,1,0,0,0,245,210,1,0,0,0,245,215,1,0,0,0,245,
  	221,1,0,0,0,245,228,1,0,0,0,245,235,1,0,0,0,245,241,1,0,0,0,245,244,1,
  	0,0,0,246,23,1,0,0,0,247,252,3,22,11,0,248,249,5,23,0,0,249,251,3,22,
  	11,0,250,248,1,0,0,0,251,254,1,0,0,0,252,250,1,0,0,0,252,253,1,0,0,0,
  	253,25,1,0,0,0,254,252,1,0,0,0,255,256,7,1,0,0,256,27,1,0,0,0,29,32,39,
  	45,49,53,57,62,65,74,78,80,90,93,99,105,115,125,138,147,151,166,170,172,
  	183,196,208,231,245,252
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

tree::TerminalNode* PccIRParser::TypeSpecContext::UNSIGNED_KW() {
  return getToken(PccIRParser::UNSIGNED_KW, 0);
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
    setState(65);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 7, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
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
      enterOuterAlt(_localctx, 2);
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
      enterOuterAlt(_localctx, 3);
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
      enterOuterAlt(_localctx, 4);
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
      enterOuterAlt(_localctx, 5);
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
    setState(67);
    typeSpec();
    setState(68);
    ref();
    setState(80);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::LBRACK: {
        setState(69);
        match(PccIRParser::LBRACK);
        setState(70);
        match(PccIRParser::NUMBER);
        setState(71);
        match(PccIRParser::RBRACK);
        setState(74);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(72);
          match(PccIRParser::ASSIGN);
          setState(73);
          initList();
        }
        break;
      }

      case PccIRParser::SEMI:
      case PccIRParser::ASSIGN: {
        setState(78);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(76);
          match(PccIRParser::ASSIGN);
          setState(77);
          match(PccIRParser::NUMBER);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(82);
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
    setState(84);
    match(PccIRParser::LBRACE);
    setState(93);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == PccIRParser::IDENT

    || _la == PccIRParser::NUMBER) {
      setState(85);
      initElem();
      setState(90);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == PccIRParser::COMMA) {
        setState(86);
        match(PccIRParser::COMMA);
        setState(87);
        initElem();
        setState(92);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(95);
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
    setState(99);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::NUMBER: {
        enterOuterAlt(_localctx, 1);
        setState(97);
        match(PccIRParser::NUMBER);
        break;
      }

      case PccIRParser::IDENT: {
        enterOuterAlt(_localctx, 2);
        setState(98);
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
    setState(101);
    typeSpec();
    setState(102);
    ref();
    setState(103);
    match(PccIRParser::LPAREN);
    setState(105);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 248) != 0)) {
      setState(104);
      paramList();
    }
    setState(107);
    match(PccIRParser::RPAREN);
    setState(108);
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
    setState(110);
    param();
    setState(115);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::COMMA) {
      setState(111);
      match(PccIRParser::COMMA);
      setState(112);
      param();
      setState(117);
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
    setState(118);
    typeSpec();
    setState(119);
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
    setState(121);
    match(PccIRParser::LBRACE);
    setState(125);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 774058271668992) != 0)) {
      setState(122);
      stmt();
      setState(127);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(128);
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
    setState(183);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::LBRACE: {
        _localctx = _tracker.createInstance<PccIRParser::BlockStmtContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(130);
        block();
        break;
      }

      case PccIRParser::IF_KW: {
        _localctx = _tracker.createInstance<PccIRParser::IfStmtContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(131);
        match(PccIRParser::IF_KW);
        setState(132);
        match(PccIRParser::LPAREN);
        setState(133);
        expr();
        setState(134);
        match(PccIRParser::RPAREN);
        setState(135);
        stmt();
        setState(138);
        _errHandler->sync(this);

        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 17, _ctx)) {
        case 1: {
          setState(136);
          match(PccIRParser::ELSE_KW);
          setState(137);
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
        setState(140);
        match(PccIRParser::WHILE_KW);
        setState(141);
        match(PccIRParser::LPAREN);
        setState(142);
        expr();
        setState(143);
        match(PccIRParser::RPAREN);
        setState(144);
        stmt();
        setState(147);
        _errHandler->sync(this);

        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 18, _ctx)) {
        case 1: {
          setState(145);
          match(PccIRParser::POST_KW);
          setState(146);
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
        setState(149);
        match(PccIRParser::RETURN_KW);
        setState(151);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 211108312973312) != 0)) {
          setState(150);
          expr();
        }
        setState(153);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::BREAK_KW: {
        _localctx = _tracker.createInstance<PccIRParser::BreakStmtContext>(_localctx);
        enterOuterAlt(_localctx, 5);
        setState(154);
        match(PccIRParser::BREAK_KW);
        setState(155);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::CONTINUE_KW: {
        _localctx = _tracker.createInstance<PccIRParser::ContinueStmtContext>(_localctx);
        enterOuterAlt(_localctx, 6);
        setState(156);
        match(PccIRParser::CONTINUE_KW);
        setState(157);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::LOCAL_KW: {
        _localctx = _tracker.createInstance<PccIRParser::LocalDeclStmtContext>(_localctx);
        enterOuterAlt(_localctx, 7);
        setState(158);
        match(PccIRParser::LOCAL_KW);
        setState(159);
        typeSpec();
        setState(160);
        ref();
        setState(172);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case PccIRParser::LBRACK: {
            setState(161);
            match(PccIRParser::LBRACK);
            setState(162);
            match(PccIRParser::NUMBER);
            setState(163);
            match(PccIRParser::RBRACK);
            setState(166);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == PccIRParser::ASSIGN) {
              setState(164);
              match(PccIRParser::ASSIGN);
              setState(165);
              initList();
            }
            break;
          }

          case PccIRParser::SEMI:
          case PccIRParser::ASSIGN: {
            setState(170);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == PccIRParser::ASSIGN) {
              setState(168);
              match(PccIRParser::ASSIGN);
              setState(169);
              expr();
            }
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        setState(174);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::ASM_BEGIN: {
        _localctx = _tracker.createInstance<PccIRParser::AsmStmtContext>(_localctx);
        enterOuterAlt(_localctx, 8);
        setState(176);
        match(PccIRParser::ASM_BEGIN);
        setState(177);
        match(PccIRParser::ASM_TEXT);
        setState(178);
        match(PccIRParser::ASM_END);
        break;
      }

      case PccIRParser::SEMI: {
        _localctx = _tracker.createInstance<PccIRParser::EmptyStmtContext>(_localctx);
        enterOuterAlt(_localctx, 9);
        setState(179);
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
        setState(180);
        expr();
        setState(181);
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
    setState(185);
    match(PccIRParser::IDENT);
    setState(186);
    match(PccIRParser::HASH);
    setState(187);
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
    setState(245);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 27, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<PccIRParser::IntLitExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(189);
      match(PccIRParser::NUMBER);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PccIRParser::IndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(190);
      ref();
      setState(191);
      match(PccIRParser::LBRACK);
      setState(192);
      expr();
      setState(193);
      match(PccIRParser::RBRACK);
      setState(196);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(194);
        match(PccIRParser::ASSIGN);
        setState(195);
        expr();
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<PccIRParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(198);
      ref();
      setState(199);
      match(PccIRParser::ASSIGN);
      setState(200);
      expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<PccIRParser::StarExprContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(202);
      match(PccIRParser::STAR);
      setState(203);
      match(PccIRParser::LPAREN);
      setState(204);
      expr();
      setState(205);
      match(PccIRParser::RPAREN);
      setState(208);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(206);
        match(PccIRParser::ASSIGN);
        setState(207);
        expr();
      }
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<PccIRParser::UnaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(210);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 2013265920) != 0))) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(211);
      match(PccIRParser::LPAREN);
      setState(212);
      expr();
      setState(213);
      match(PccIRParser::RPAREN);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<PccIRParser::BinaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(215);
      match(PccIRParser::LPAREN);
      setState(216);
      expr();
      setState(217);
      binOp();
      setState(218);
      expr();
      setState(219);
      match(PccIRParser::RPAREN);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<PccIRParser::ConvertExprContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(221);
      match(PccIRParser::LPAREN);
      setState(222);
      typeSpec();
      setState(223);
      match(PccIRParser::RPAREN);
      setState(224);
      match(PccIRParser::LPAREN);
      setState(225);
      expr();
      setState(226);
      match(PccIRParser::RPAREN);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<PccIRParser::CallExprContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(228);
      ref();
      setState(229);
      match(PccIRParser::LPAREN);
      setState(231);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 211108312973312) != 0)) {
        setState(230);
        argList();
      }
      setState(233);
      match(PccIRParser::RPAREN);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<PccIRParser::CallPtrExprContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(235);
      match(PccIRParser::LPAREN);
      setState(236);
      expr();
      setState(237);
      match(PccIRParser::RPAREN);
      setState(238);
      match(PccIRParser::LPAREN);
      setState(239);
      match(PccIRParser::RPAREN);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<PccIRParser::IntrinsicExprContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(241);
      match(PccIRParser::IDENT);
      setState(242);
      match(PccIRParser::LPAREN);
      setState(243);
      match(PccIRParser::RPAREN);
      break;
    }

    case 11: {
      _localctx = _tracker.createInstance<PccIRParser::VarExprContext>(_localctx);
      enterOuterAlt(_localctx, 11);
      setState(244);
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
    setState(247);
    expr();
    setState(252);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::COMMA) {
      setState(248);
      match(PccIRParser::COMMA);
      setState(249);
      expr();
      setState(254);
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
    setState(255);
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
