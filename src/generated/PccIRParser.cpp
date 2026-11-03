
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
      "program", "globalDecl", "initList", "initElem", "funcDecl", "paramList", 
      "param", "block", "stmt", "ref", "expr", "argList", "binOp"
    },
    std::vector<std::string>{
      "", "'; --- globals ---'", "'; --- functions ---'", "'int'", "'local'", 
      "'if'", "'else'", "'while'", "'return'", "'('", "')'", "'['", "']'", 
      "'{'", "", "';'", "','", "'='", "'#'", "'*'", "'&'", "'-'", "'!'", 
      "'~'", "'/'", "'%'", "'+'", "'<<'", "'>>'", "'<='", "'>='", "'<'", 
      "'>'", "'=='", "'!='", "'^'", "'|'", "'&&'", "'||'"
    },
    std::vector<std::string>{
      "", "GLOBALS_HEADER", "FUNCTIONS_HEADER", "INT_KW", "LOCAL_KW", "IF_KW", 
      "ELSE_KW", "WHILE_KW", "RETURN_KW", "LPAREN", "RPAREN", "LBRACK", 
      "RBRACK", "LBRACE", "RBRACE", "SEMI", "COMMA", "ASSIGN", "HASH", "STAR", 
      "AMP", "MINUS", "BANG", "TILDE", "SLASH", "PERCENT", "PLUS", "SHL", 
      "SHR", "LE", "GE", "LT", "GT", "EQ", "NE", "CARET", "PIPE", "ANDAND", 
      "OROR", "IDENT", "NUMBER", "WS", "ASM_BEGIN", "ASM_TEXT", "ASM_END"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,44,218,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,1,0,1,0,5,0,29,8,
  	0,10,0,12,0,32,9,0,1,0,1,0,5,0,36,8,0,10,0,12,0,39,9,0,1,0,1,0,1,1,1,
  	1,1,1,1,1,1,1,1,1,1,1,3,1,50,8,1,1,1,1,1,3,1,54,8,1,3,1,56,8,1,1,1,1,
  	1,1,2,1,2,1,2,1,2,5,2,64,8,2,10,2,12,2,67,9,2,3,2,69,8,2,1,2,1,2,1,3,
  	1,3,3,3,75,8,3,1,4,1,4,1,4,1,4,3,4,81,8,4,1,4,1,4,1,4,1,5,1,5,1,5,5,5,
  	89,8,5,10,5,12,5,92,9,5,1,6,1,6,1,6,1,7,1,7,5,7,99,8,7,10,7,12,7,102,
  	9,7,1,7,1,7,1,8,1,8,1,8,1,8,1,8,1,8,1,8,1,8,3,8,114,8,8,1,8,1,8,1,8,1,
  	8,1,8,1,8,1,8,1,8,3,8,124,8,8,1,8,1,8,1,8,1,8,1,8,1,8,1,8,1,8,3,8,134,
  	8,8,1,8,1,8,3,8,138,8,8,3,8,140,8,8,1,8,1,8,1,8,1,8,1,8,1,8,1,8,1,8,1,
  	8,3,8,151,8,8,1,9,1,9,1,9,1,9,1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,
  	164,8,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,176,8,
  	10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,
  	10,3,10,192,8,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1,
  	10,1,10,3,10,206,8,10,1,11,1,11,1,11,5,11,211,8,11,10,11,12,11,214,9,
  	11,1,12,1,12,1,12,0,0,13,0,2,4,6,8,10,12,14,16,18,20,22,24,0,2,1,0,20,
  	23,2,0,19,21,24,38,240,0,26,1,0,0,0,2,42,1,0,0,0,4,59,1,0,0,0,6,74,1,
  	0,0,0,8,76,1,0,0,0,10,85,1,0,0,0,12,93,1,0,0,0,14,96,1,0,0,0,16,150,1,
  	0,0,0,18,152,1,0,0,0,20,205,1,0,0,0,22,207,1,0,0,0,24,215,1,0,0,0,26,
  	30,5,1,0,0,27,29,3,2,1,0,28,27,1,0,0,0,29,32,1,0,0,0,30,28,1,0,0,0,30,
  	31,1,0,0,0,31,33,1,0,0,0,32,30,1,0,0,0,33,37,5,2,0,0,34,36,3,8,4,0,35,
  	34,1,0,0,0,36,39,1,0,0,0,37,35,1,0,0,0,37,38,1,0,0,0,38,40,1,0,0,0,39,
  	37,1,0,0,0,40,41,5,0,0,1,41,1,1,0,0,0,42,43,5,3,0,0,43,55,3,18,9,0,44,
  	45,5,11,0,0,45,46,5,40,0,0,46,49,5,12,0,0,47,48,5,17,0,0,48,50,3,4,2,
  	0,49,47,1,0,0,0,49,50,1,0,0,0,50,56,1,0,0,0,51,52,5,17,0,0,52,54,5,40,
  	0,0,53,51,1,0,0,0,53,54,1,0,0,0,54,56,1,0,0,0,55,44,1,0,0,0,55,53,1,0,
  	0,0,56,57,1,0,0,0,57,58,5,15,0,0,58,3,1,0,0,0,59,68,5,13,0,0,60,65,3,
  	6,3,0,61,62,5,16,0,0,62,64,3,6,3,0,63,61,1,0,0,0,64,67,1,0,0,0,65,63,
  	1,0,0,0,65,66,1,0,0,0,66,69,1,0,0,0,67,65,1,0,0,0,68,60,1,0,0,0,68,69,
  	1,0,0,0,69,70,1,0,0,0,70,71,5,14,0,0,71,5,1,0,0,0,72,75,5,40,0,0,73,75,
  	3,18,9,0,74,72,1,0,0,0,74,73,1,0,0,0,75,7,1,0,0,0,76,77,5,3,0,0,77,78,
  	3,18,9,0,78,80,5,9,0,0,79,81,3,10,5,0,80,79,1,0,0,0,80,81,1,0,0,0,81,
  	82,1,0,0,0,82,83,5,10,0,0,83,84,3,14,7,0,84,9,1,0,0,0,85,90,3,12,6,0,
  	86,87,5,16,0,0,87,89,3,12,6,0,88,86,1,0,0,0,89,92,1,0,0,0,90,88,1,0,0,
  	0,90,91,1,0,0,0,91,11,1,0,0,0,92,90,1,0,0,0,93,94,5,3,0,0,94,95,3,18,
  	9,0,95,13,1,0,0,0,96,100,5,13,0,0,97,99,3,16,8,0,98,97,1,0,0,0,99,102,
  	1,0,0,0,100,98,1,0,0,0,100,101,1,0,0,0,101,103,1,0,0,0,102,100,1,0,0,
  	0,103,104,5,14,0,0,104,15,1,0,0,0,105,151,3,14,7,0,106,107,5,5,0,0,107,
  	108,5,9,0,0,108,109,3,20,10,0,109,110,5,10,0,0,110,113,3,16,8,0,111,112,
  	5,6,0,0,112,114,3,16,8,0,113,111,1,0,0,0,113,114,1,0,0,0,114,151,1,0,
  	0,0,115,116,5,7,0,0,116,117,5,9,0,0,117,118,3,20,10,0,118,119,5,10,0,
  	0,119,120,3,16,8,0,120,151,1,0,0,0,121,123,5,8,0,0,122,124,3,20,10,0,
  	123,122,1,0,0,0,123,124,1,0,0,0,124,125,1,0,0,0,125,151,5,15,0,0,126,
  	127,5,4,0,0,127,139,3,18,9,0,128,129,5,11,0,0,129,130,5,40,0,0,130,133,
  	5,12,0,0,131,132,5,17,0,0,132,134,3,4,2,0,133,131,1,0,0,0,133,134,1,0,
  	0,0,134,140,1,0,0,0,135,136,5,17,0,0,136,138,3,20,10,0,137,135,1,0,0,
  	0,137,138,1,0,0,0,138,140,1,0,0,0,139,128,1,0,0,0,139,137,1,0,0,0,140,
  	141,1,0,0,0,141,142,5,15,0,0,142,151,1,0,0,0,143,144,5,42,0,0,144,145,
  	5,43,0,0,145,151,5,44,0,0,146,151,5,15,0,0,147,148,3,20,10,0,148,149,
  	5,15,0,0,149,151,1,0,0,0,150,105,1,0,0,0,150,106,1,0,0,0,150,115,1,0,
  	0,0,150,121,1,0,0,0,150,126,1,0,0,0,150,143,1,0,0,0,150,146,1,0,0,0,150,
  	147,1,0,0,0,151,17,1,0,0,0,152,153,5,39,0,0,153,154,5,18,0,0,154,155,
  	5,40,0,0,155,19,1,0,0,0,156,206,5,40,0,0,157,158,3,18,9,0,158,159,5,11,
  	0,0,159,160,3,20,10,0,160,163,5,12,0,0,161,162,5,17,0,0,162,164,3,20,
  	10,0,163,161,1,0,0,0,163,164,1,0,0,0,164,206,1,0,0,0,165,166,3,18,9,0,
  	166,167,5,17,0,0,167,168,3,20,10,0,168,206,1,0,0,0,169,170,5,19,0,0,170,
  	171,5,9,0,0,171,172,3,20,10,0,172,175,5,10,0,0,173,174,5,17,0,0,174,176,
  	3,20,10,0,175,173,1,0,0,0,175,176,1,0,0,0,176,206,1,0,0,0,177,178,7,0,
  	0,0,178,179,5,9,0,0,179,180,3,20,10,0,180,181,5,10,0,0,181,206,1,0,0,
  	0,182,183,5,9,0,0,183,184,3,20,10,0,184,185,3,24,12,0,185,186,3,20,10,
  	0,186,187,5,10,0,0,187,206,1,0,0,0,188,189,3,18,9,0,189,191,5,9,0,0,190,
  	192,3,22,11,0,191,190,1,0,0,0,191,192,1,0,0,0,192,193,1,0,0,0,193,194,
  	5,10,0,0,194,206,1,0,0,0,195,196,5,9,0,0,196,197,3,20,10,0,197,198,5,
  	10,0,0,198,199,5,9,0,0,199,200,5,10,0,0,200,206,1,0,0,0,201,202,5,39,
  	0,0,202,203,5,9,0,0,203,206,5,10,0,0,204,206,3,18,9,0,205,156,1,0,0,0,
  	205,157,1,0,0,0,205,165,1,0,0,0,205,169,1,0,0,0,205,177,1,0,0,0,205,182,
  	1,0,0,0,205,188,1,0,0,0,205,195,1,0,0,0,205,201,1,0,0,0,205,204,1,0,0,
  	0,206,21,1,0,0,0,207,212,3,20,10,0,208,209,5,16,0,0,209,211,3,20,10,0,
  	210,208,1,0,0,0,211,214,1,0,0,0,212,210,1,0,0,0,212,213,1,0,0,0,213,23,
  	1,0,0,0,214,212,1,0,0,0,215,216,7,1,0,0,216,25,1,0,0,0,22,30,37,49,53,
  	55,65,68,74,80,90,100,113,123,133,137,139,150,163,175,191,205,212
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
    setState(26);
    match(PccIRParser::GLOBALS_HEADER);
    setState(30);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::INT_KW) {
      setState(27);
      globalDecl();
      setState(32);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(33);
    match(PccIRParser::FUNCTIONS_HEADER);
    setState(37);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::INT_KW) {
      setState(34);
      funcDecl();
      setState(39);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(40);
    match(PccIRParser::EOF);
   
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

tree::TerminalNode* PccIRParser::GlobalDeclContext::INT_KW() {
  return getToken(PccIRParser::INT_KW, 0);
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
  enterRule(_localctx, 2, PccIRParser::RuleGlobalDecl);
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
    match(PccIRParser::INT_KW);
    setState(43);
    ref();
    setState(55);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::LBRACK: {
        setState(44);
        match(PccIRParser::LBRACK);
        setState(45);
        match(PccIRParser::NUMBER);
        setState(46);
        match(PccIRParser::RBRACK);
        setState(49);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(47);
          match(PccIRParser::ASSIGN);
          setState(48);
          initList();
        }
        break;
      }

      case PccIRParser::SEMI:
      case PccIRParser::ASSIGN: {
        setState(53);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == PccIRParser::ASSIGN) {
          setState(51);
          match(PccIRParser::ASSIGN);
          setState(52);
          match(PccIRParser::NUMBER);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(57);
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
  enterRule(_localctx, 4, PccIRParser::RuleInitList);
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
    setState(59);
    match(PccIRParser::LBRACE);
    setState(68);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == PccIRParser::IDENT

    || _la == PccIRParser::NUMBER) {
      setState(60);
      initElem();
      setState(65);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == PccIRParser::COMMA) {
        setState(61);
        match(PccIRParser::COMMA);
        setState(62);
        initElem();
        setState(67);
        _errHandler->sync(this);
        _la = _input->LA(1);
      }
    }
    setState(70);
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
  enterRule(_localctx, 6, PccIRParser::RuleInitElem);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(74);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::NUMBER: {
        enterOuterAlt(_localctx, 1);
        setState(72);
        match(PccIRParser::NUMBER);
        break;
      }

      case PccIRParser::IDENT: {
        enterOuterAlt(_localctx, 2);
        setState(73);
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

tree::TerminalNode* PccIRParser::FuncDeclContext::INT_KW() {
  return getToken(PccIRParser::INT_KW, 0);
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
  enterRule(_localctx, 8, PccIRParser::RuleFuncDecl);
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
    setState(76);
    match(PccIRParser::INT_KW);
    setState(77);
    ref();
    setState(78);
    match(PccIRParser::LPAREN);
    setState(80);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == PccIRParser::INT_KW) {
      setState(79);
      paramList();
    }
    setState(82);
    match(PccIRParser::RPAREN);
    setState(83);
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
  enterRule(_localctx, 10, PccIRParser::RuleParamList);
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
    setState(85);
    param();
    setState(90);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::COMMA) {
      setState(86);
      match(PccIRParser::COMMA);
      setState(87);
      param();
      setState(92);
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

tree::TerminalNode* PccIRParser::ParamContext::INT_KW() {
  return getToken(PccIRParser::INT_KW, 0);
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
  enterRule(_localctx, 12, PccIRParser::RuleParam);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(93);
    match(PccIRParser::INT_KW);
    setState(94);
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
  enterRule(_localctx, 14, PccIRParser::RuleBlock);
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
    match(PccIRParser::LBRACE);
    setState(100);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6047330247600) != 0)) {
      setState(97);
      stmt();
      setState(102);
      _errHandler->sync(this);
      _la = _input->LA(1);
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

PccIRParser::StmtContext* PccIRParser::WhileStmtContext::stmt() {
  return getRuleContext<PccIRParser::StmtContext>(0);
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
PccIRParser::StmtContext* PccIRParser::stmt() {
  StmtContext *_localctx = _tracker.createInstance<StmtContext>(_ctx, getState());
  enterRule(_localctx, 16, PccIRParser::RuleStmt);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(150);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccIRParser::LBRACE: {
        _localctx = _tracker.createInstance<PccIRParser::BlockStmtContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(105);
        block();
        break;
      }

      case PccIRParser::IF_KW: {
        _localctx = _tracker.createInstance<PccIRParser::IfStmtContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(106);
        match(PccIRParser::IF_KW);
        setState(107);
        match(PccIRParser::LPAREN);
        setState(108);
        expr();
        setState(109);
        match(PccIRParser::RPAREN);
        setState(110);
        stmt();
        setState(113);
        _errHandler->sync(this);

        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 11, _ctx)) {
        case 1: {
          setState(111);
          match(PccIRParser::ELSE_KW);
          setState(112);
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
        setState(115);
        match(PccIRParser::WHILE_KW);
        setState(116);
        match(PccIRParser::LPAREN);
        setState(117);
        expr();
        setState(118);
        match(PccIRParser::RPAREN);
        setState(119);
        stmt();
        break;
      }

      case PccIRParser::RETURN_KW: {
        _localctx = _tracker.createInstance<PccIRParser::ReturnStmtContext>(_localctx);
        enterOuterAlt(_localctx, 4);
        setState(121);
        match(PccIRParser::RETURN_KW);
        setState(123);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 1649283695104) != 0)) {
          setState(122);
          expr();
        }
        setState(125);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::LOCAL_KW: {
        _localctx = _tracker.createInstance<PccIRParser::LocalDeclStmtContext>(_localctx);
        enterOuterAlt(_localctx, 5);
        setState(126);
        match(PccIRParser::LOCAL_KW);
        setState(127);
        ref();
        setState(139);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case PccIRParser::LBRACK: {
            setState(128);
            match(PccIRParser::LBRACK);
            setState(129);
            match(PccIRParser::NUMBER);
            setState(130);
            match(PccIRParser::RBRACK);
            setState(133);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == PccIRParser::ASSIGN) {
              setState(131);
              match(PccIRParser::ASSIGN);
              setState(132);
              initList();
            }
            break;
          }

          case PccIRParser::SEMI:
          case PccIRParser::ASSIGN: {
            setState(137);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == PccIRParser::ASSIGN) {
              setState(135);
              match(PccIRParser::ASSIGN);
              setState(136);
              expr();
            }
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        setState(141);
        match(PccIRParser::SEMI);
        break;
      }

      case PccIRParser::ASM_BEGIN: {
        _localctx = _tracker.createInstance<PccIRParser::AsmStmtContext>(_localctx);
        enterOuterAlt(_localctx, 6);
        setState(143);
        match(PccIRParser::ASM_BEGIN);
        setState(144);
        match(PccIRParser::ASM_TEXT);
        setState(145);
        match(PccIRParser::ASM_END);
        break;
      }

      case PccIRParser::SEMI: {
        _localctx = _tracker.createInstance<PccIRParser::EmptyStmtContext>(_localctx);
        enterOuterAlt(_localctx, 7);
        setState(146);
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
        enterOuterAlt(_localctx, 8);
        setState(147);
        expr();
        setState(148);
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
  enterRule(_localctx, 18, PccIRParser::RuleRef);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(152);
    match(PccIRParser::IDENT);
    setState(153);
    match(PccIRParser::HASH);
    setState(154);
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
  enterRule(_localctx, 20, PccIRParser::RuleExpr);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(205);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 20, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<PccIRParser::IntLitExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(156);
      match(PccIRParser::NUMBER);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PccIRParser::IndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(157);
      ref();
      setState(158);
      match(PccIRParser::LBRACK);
      setState(159);
      expr();
      setState(160);
      match(PccIRParser::RBRACK);
      setState(163);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(161);
        match(PccIRParser::ASSIGN);
        setState(162);
        expr();
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<PccIRParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(165);
      ref();
      setState(166);
      match(PccIRParser::ASSIGN);
      setState(167);
      expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<PccIRParser::StarExprContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(169);
      match(PccIRParser::STAR);
      setState(170);
      match(PccIRParser::LPAREN);
      setState(171);
      expr();
      setState(172);
      match(PccIRParser::RPAREN);
      setState(175);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == PccIRParser::ASSIGN) {
        setState(173);
        match(PccIRParser::ASSIGN);
        setState(174);
        expr();
      }
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<PccIRParser::UnaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 5);
      setState(177);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 15728640) != 0))) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(178);
      match(PccIRParser::LPAREN);
      setState(179);
      expr();
      setState(180);
      match(PccIRParser::RPAREN);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<PccIRParser::BinaryExprContext>(_localctx);
      enterOuterAlt(_localctx, 6);
      setState(182);
      match(PccIRParser::LPAREN);
      setState(183);
      expr();
      setState(184);
      binOp();
      setState(185);
      expr();
      setState(186);
      match(PccIRParser::RPAREN);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<PccIRParser::CallExprContext>(_localctx);
      enterOuterAlt(_localctx, 7);
      setState(188);
      ref();
      setState(189);
      match(PccIRParser::LPAREN);
      setState(191);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 1649283695104) != 0)) {
        setState(190);
        argList();
      }
      setState(193);
      match(PccIRParser::RPAREN);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<PccIRParser::CallPtrExprContext>(_localctx);
      enterOuterAlt(_localctx, 8);
      setState(195);
      match(PccIRParser::LPAREN);
      setState(196);
      expr();
      setState(197);
      match(PccIRParser::RPAREN);
      setState(198);
      match(PccIRParser::LPAREN);
      setState(199);
      match(PccIRParser::RPAREN);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<PccIRParser::IntrinsicExprContext>(_localctx);
      enterOuterAlt(_localctx, 9);
      setState(201);
      match(PccIRParser::IDENT);
      setState(202);
      match(PccIRParser::LPAREN);
      setState(203);
      match(PccIRParser::RPAREN);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<PccIRParser::VarExprContext>(_localctx);
      enterOuterAlt(_localctx, 10);
      setState(204);
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
  enterRule(_localctx, 22, PccIRParser::RuleArgList);
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
    setState(207);
    expr();
    setState(212);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == PccIRParser::COMMA) {
      setState(208);
      match(PccIRParser::COMMA);
      setState(209);
      expr();
      setState(214);
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
  enterRule(_localctx, 24, PccIRParser::RuleBinOp);
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
    setState(215);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 549742706688) != 0))) {
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
