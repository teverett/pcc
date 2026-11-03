
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
      "program", "topLevelDecl", "functionDecl", "paramList", "param", "globalDecl", 
      "arrayInit", "arrayInitElem", "block", "statement", "simpleStmt", 
      "expr", "condExpr", "argList"
    },
    std::vector<std::string>{
      "", "'int'", "'if'", "'else'", "'while'", "'for'", "'return'", "'('", 
      "')'", "'['", "']'", "'{'", "", "'@'", "';'", "','", "'='", "'*'", 
      "'&'", "'-'", "'!'", "'~'", "'/'", "'%'", "'+'", "'<<'", "'>>'", "'<='", 
      "'>='", "'<'", "'>'", "'=='", "'!='", "'^'", "'|'", "'&&'", "'||'"
    },
    std::vector<std::string>{
      "", "INT_KW", "IF_KW", "ELSE_KW", "WHILE_KW", "FOR_KW", "RETURN_KW", 
      "LPAREN", "RPAREN", "LBRACK", "RBRACK", "LBRACE", "RBRACE", "AT", 
      "SEMI", "COMMA", "ASSIGN", "STAR", "AMP", "MINUS", "BANG", "TILDE", 
      "SLASH", "PERCENT", "PLUS", "SHL", "SHR", "LE", "GE", "LT", "GT", 
      "EQ", "NE", "CARET", "PIPE", "ANDAND", "OROR", "IDENT", "NUMBER", 
      "CHARLIT", "LINE_COMMENT", "BLOCK_COMMENT", "WS", "ASM_BEGIN", "ASM_TEXT", 
      "ASM_END"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,45,283,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,1,0,5,0,
  	30,8,0,10,0,12,0,33,9,0,1,0,1,0,1,1,1,1,3,1,39,8,1,1,2,1,2,1,2,1,2,3,
  	2,45,8,2,1,2,1,2,1,2,3,2,50,8,2,1,2,1,2,3,2,54,8,2,1,3,1,3,1,3,5,3,59,
  	8,3,10,3,12,3,62,9,3,1,4,1,4,1,4,1,5,1,5,1,5,1,5,1,5,1,5,1,5,3,5,74,8,
  	5,1,5,1,5,3,5,78,8,5,1,5,1,5,1,5,1,5,1,5,3,5,85,8,5,1,5,3,5,88,8,5,1,
  	5,1,5,3,5,92,8,5,1,5,3,5,95,8,5,1,6,1,6,1,6,1,6,5,6,101,8,6,10,6,12,6,
  	104,9,6,1,6,1,6,1,7,1,7,3,7,110,8,7,1,8,1,8,5,8,114,8,8,10,8,12,8,117,
  	9,8,1,8,1,8,1,9,1,9,1,9,1,9,1,9,1,9,1,9,1,9,3,9,129,8,9,1,9,1,9,1,9,1,
  	9,1,9,1,9,1,9,1,9,1,9,3,9,140,8,9,1,9,1,9,3,9,144,8,9,1,9,1,9,3,9,148,
  	8,9,1,9,1,9,1,9,1,9,3,9,154,8,9,1,9,1,9,1,9,3,9,159,8,9,1,9,1,9,1,9,1,
  	9,1,9,3,9,166,8,9,1,10,1,10,1,10,1,10,1,10,1,10,1,10,3,10,175,8,10,1,
  	10,1,10,1,10,1,10,3,10,181,8,10,1,10,3,10,184,8,10,1,11,1,11,1,11,1,11,
  	1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,1,11,3,11,202,
  	8,11,1,12,1,12,1,12,1,12,3,12,208,8,12,1,12,1,12,1,12,1,12,1,12,1,12,
  	3,12,216,8,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,
  	1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,3,12,238,8,12,1,12,1,12,
  	1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,
  	1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,1,12,
  	5,12,270,8,12,10,12,12,12,273,9,12,1,13,1,13,1,13,5,13,278,8,13,10,13,
  	12,13,281,9,13,1,13,0,1,24,14,0,2,4,6,8,10,12,14,16,18,20,22,24,26,0,
  	6,1,0,19,21,2,0,17,17,22,23,2,0,19,19,24,24,1,0,25,26,1,0,27,30,1,0,31,
  	32,325,0,31,1,0,0,0,2,38,1,0,0,0,4,40,1,0,0,0,6,55,1,0,0,0,8,63,1,0,0,
  	0,10,94,1,0,0,0,12,96,1,0,0,0,14,109,1,0,0,0,16,111,1,0,0,0,18,165,1,
  	0,0,0,20,183,1,0,0,0,22,201,1,0,0,0,24,237,1,0,0,0,26,274,1,0,0,0,28,
  	30,3,2,1,0,29,28,1,0,0,0,30,33,1,0,0,0,31,29,1,0,0,0,31,32,1,0,0,0,32,
  	34,1,0,0,0,33,31,1,0,0,0,34,35,5,0,0,1,35,1,1,0,0,0,36,39,3,4,2,0,37,
  	39,3,10,5,0,38,36,1,0,0,0,38,37,1,0,0,0,39,3,1,0,0,0,40,41,5,1,0,0,41,
  	42,5,37,0,0,42,44,5,7,0,0,43,45,3,6,3,0,44,43,1,0,0,0,44,45,1,0,0,0,45,
  	46,1,0,0,0,46,49,5,8,0,0,47,48,5,13,0,0,48,50,5,38,0,0,49,47,1,0,0,0,
  	49,50,1,0,0,0,50,53,1,0,0,0,51,54,3,16,8,0,52,54,5,14,0,0,53,51,1,0,0,
  	0,53,52,1,0,0,0,54,5,1,0,0,0,55,60,3,8,4,0,56,57,5,15,0,0,57,59,3,8,4,
  	0,58,56,1,0,0,0,59,62,1,0,0,0,60,58,1,0,0,0,60,61,1,0,0,0,61,7,1,0,0,
  	0,62,60,1,0,0,0,63,64,5,1,0,0,64,65,5,37,0,0,65,9,1,0,0,0,66,67,5,1,0,
  	0,67,68,5,37,0,0,68,69,5,9,0,0,69,70,5,38,0,0,70,73,5,10,0,0,71,72,5,
  	16,0,0,72,74,3,12,6,0,73,71,1,0,0,0,73,74,1,0,0,0,74,77,1,0,0,0,75,76,
  	5,13,0,0,76,78,5,38,0,0,77,75,1,0,0,0,77,78,1,0,0,0,78,79,1,0,0,0,79,
  	95,5,14,0,0,80,81,5,1,0,0,81,87,5,37,0,0,82,84,5,16,0,0,83,85,5,19,0,
  	0,84,83,1,0,0,0,84,85,1,0,0,0,85,86,1,0,0,0,86,88,5,38,0,0,87,82,1,0,
  	0,0,87,88,1,0,0,0,88,91,1,0,0,0,89,90,5,13,0,0,90,92,5,38,0,0,91,89,1,
  	0,0,0,91,92,1,0,0,0,92,93,1,0,0,0,93,95,5,14,0,0,94,66,1,0,0,0,94,80,
  	1,0,0,0,95,11,1,0,0,0,96,97,5,11,0,0,97,102,3,14,7,0,98,99,5,15,0,0,99,
  	101,3,14,7,0,100,98,1,0,0,0,101,104,1,0,0,0,102,100,1,0,0,0,102,103,1,
  	0,0,0,103,105,1,0,0,0,104,102,1,0,0,0,105,106,5,12,0,0,106,13,1,0,0,0,
  	107,110,5,38,0,0,108,110,5,37,0,0,109,107,1,0,0,0,109,108,1,0,0,0,110,
  	15,1,0,0,0,111,115,5,11,0,0,112,114,3,18,9,0,113,112,1,0,0,0,114,117,
  	1,0,0,0,115,113,1,0,0,0,115,116,1,0,0,0,116,118,1,0,0,0,117,115,1,0,0,
  	0,118,119,5,12,0,0,119,17,1,0,0,0,120,166,3,16,8,0,121,122,5,2,0,0,122,
  	123,5,7,0,0,123,124,3,22,11,0,124,125,5,8,0,0,125,128,3,18,9,0,126,127,
  	5,3,0,0,127,129,3,18,9,0,128,126,1,0,0,0,128,129,1,0,0,0,129,166,1,0,
  	0,0,130,131,5,4,0,0,131,132,5,7,0,0,132,133,3,22,11,0,133,134,5,8,0,0,
  	134,135,3,18,9,0,135,166,1,0,0,0,136,137,5,5,0,0,137,139,5,7,0,0,138,
  	140,3,20,10,0,139,138,1,0,0,0,139,140,1,0,0,0,140,141,1,0,0,0,141,143,
  	5,14,0,0,142,144,3,22,11,0,143,142,1,0,0,0,143,144,1,0,0,0,144,145,1,
  	0,0,0,145,147,5,14,0,0,146,148,3,20,10,0,147,146,1,0,0,0,147,148,1,0,
  	0,0,148,149,1,0,0,0,149,150,5,8,0,0,150,166,3,18,9,0,151,153,5,6,0,0,
  	152,154,3,22,11,0,153,152,1,0,0,0,153,154,1,0,0,0,154,155,1,0,0,0,155,
  	166,5,14,0,0,156,158,5,43,0,0,157,159,5,44,0,0,158,157,1,0,0,0,158,159,
  	1,0,0,0,159,160,1,0,0,0,160,166,5,45,0,0,161,162,3,20,10,0,162,163,5,
  	14,0,0,163,166,1,0,0,0,164,166,5,14,0,0,165,120,1,0,0,0,165,121,1,0,0,
  	0,165,130,1,0,0,0,165,136,1,0,0,0,165,151,1,0,0,0,165,156,1,0,0,0,165,
  	161,1,0,0,0,165,164,1,0,0,0,166,19,1,0,0,0,167,168,5,1,0,0,168,169,5,
  	37,0,0,169,170,5,9,0,0,170,171,5,38,0,0,171,174,5,10,0,0,172,173,5,16,
  	0,0,173,175,3,12,6,0,174,172,1,0,0,0,174,175,1,0,0,0,175,184,1,0,0,0,
  	176,177,5,1,0,0,177,180,5,37,0,0,178,179,5,16,0,0,179,181,3,22,11,0,180,
  	178,1,0,0,0,180,181,1,0,0,0,181,184,1,0,0,0,182,184,3,22,11,0,183,167,
  	1,0,0,0,183,176,1,0,0,0,183,182,1,0,0,0,184,21,1,0,0,0,185,186,5,17,0,
  	0,186,187,3,24,12,0,187,188,5,16,0,0,188,189,3,22,11,0,189,202,1,0,0,
  	0,190,191,5,37,0,0,191,192,5,9,0,0,192,193,3,22,11,0,193,194,5,10,0,0,
  	194,195,5,16,0,0,195,196,3,22,11,0,196,202,1,0,0,0,197,198,5,37,0,0,198,
  	199,5,16,0,0,199,202,3,22,11,0,200,202,3,24,12,0,201,185,1,0,0,0,201,
  	190,1,0,0,0,201,197,1,0,0,0,201,200,1,0,0,0,202,23,1,0,0,0,203,204,6,
  	12,-1,0,204,205,5,37,0,0,205,207,5,7,0,0,206,208,3,26,13,0,207,206,1,
  	0,0,0,207,208,1,0,0,0,208,209,1,0,0,0,209,238,5,8,0,0,210,211,5,7,0,0,
  	211,212,3,22,11,0,212,213,5,8,0,0,213,215,5,7,0,0,214,216,3,26,13,0,215,
  	214,1,0,0,0,215,216,1,0,0,0,216,217,1,0,0,0,217,218,5,8,0,0,218,238,1,
  	0,0,0,219,220,5,37,0,0,220,221,5,9,0,0,221,222,3,22,11,0,222,223,5,10,
  	0,0,223,238,1,0,0,0,224,238,5,37,0,0,225,238,5,38,0,0,226,238,5,39,0,
  	0,227,228,5,7,0,0,228,229,3,22,11,0,229,230,5,8,0,0,230,238,1,0,0,0,231,
  	232,5,17,0,0,232,238,3,24,12,13,233,234,5,18,0,0,234,238,3,24,12,12,235,
  	236,7,0,0,0,236,238,3,24,12,11,237,203,1,0,0,0,237,210,1,0,0,0,237,219,
  	1,0,0,0,237,224,1,0,0,0,237,225,1,0,0,0,237,226,1,0,0,0,237,227,1,0,0,
  	0,237,231,1,0,0,0,237,233,1,0,0,0,237,235,1,0,0,0,238,271,1,0,0,0,239,
  	240,10,10,0,0,240,241,7,1,0,0,241,270,3,24,12,11,242,243,10,9,0,0,243,
  	244,7,2,0,0,244,270,3,24,12,10,245,246,10,8,0,0,246,247,7,3,0,0,247,270,
  	3,24,12,9,248,249,10,7,0,0,249,250,7,4,0,0,250,270,3,24,12,8,251,252,
  	10,6,0,0,252,253,7,5,0,0,253,270,3,24,12,7,254,255,10,5,0,0,255,256,5,
  	18,0,0,256,270,3,24,12,6,257,258,10,4,0,0,258,259,5,33,0,0,259,270,3,
  	24,12,5,260,261,10,3,0,0,261,262,5,34,0,0,262,270,3,24,12,4,263,264,10,
  	2,0,0,264,265,5,35,0,0,265,270,3,24,12,3,266,267,10,1,0,0,267,268,5,36,
  	0,0,268,270,3,24,12,2,269,239,1,0,0,0,269,242,1,0,0,0,269,245,1,0,0,0,
  	269,248,1,0,0,0,269,251,1,0,0,0,269,254,1,0,0,0,269,257,1,0,0,0,269,260,
  	1,0,0,0,269,263,1,0,0,0,269,266,1,0,0,0,270,273,1,0,0,0,271,269,1,0,0,
  	0,271,272,1,0,0,0,272,25,1,0,0,0,273,271,1,0,0,0,274,279,3,22,11,0,275,
  	276,5,15,0,0,276,278,3,22,11,0,277,275,1,0,0,0,278,281,1,0,0,0,279,277,
  	1,0,0,0,279,280,1,0,0,0,280,27,1,0,0,0,281,279,1,0,0,0,32,31,38,44,49,
  	53,60,73,77,84,87,91,94,102,109,115,128,139,143,147,153,158,165,174,180,
  	183,201,207,215,237,269,271,279
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
    setState(31);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::INT_KW) {
      setState(28);
      topLevelDecl();
      setState(33);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(34);
    match(CParser::EOF);
   
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
  enterRule(_localctx, 2, CParser::RuleTopLevelDecl);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(38);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 1, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(36);
      functionDecl();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(37);
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

tree::TerminalNode* CParser::FunctionDeclContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
}

tree::TerminalNode* CParser::FunctionDeclContext::IDENT() {
  return getToken(CParser::IDENT, 0);
}

tree::TerminalNode* CParser::FunctionDeclContext::LPAREN() {
  return getToken(CParser::LPAREN, 0);
}

tree::TerminalNode* CParser::FunctionDeclContext::RPAREN() {
  return getToken(CParser::RPAREN, 0);
}

CParser::BlockContext* CParser::FunctionDeclContext::block() {
  return getRuleContext<CParser::BlockContext>(0);
}

tree::TerminalNode* CParser::FunctionDeclContext::SEMI() {
  return getToken(CParser::SEMI, 0);
}

CParser::ParamListContext* CParser::FunctionDeclContext::paramList() {
  return getRuleContext<CParser::ParamListContext>(0);
}

tree::TerminalNode* CParser::FunctionDeclContext::AT() {
  return getToken(CParser::AT, 0);
}

tree::TerminalNode* CParser::FunctionDeclContext::NUMBER() {
  return getToken(CParser::NUMBER, 0);
}


size_t CParser::FunctionDeclContext::getRuleIndex() const {
  return CParser::RuleFunctionDecl;
}


std::any CParser::FunctionDeclContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitFunctionDecl(this);
  else
    return visitor->visitChildren(this);
}

CParser::FunctionDeclContext* CParser::functionDecl() {
  FunctionDeclContext *_localctx = _tracker.createInstance<FunctionDeclContext>(_ctx, getState());
  enterRule(_localctx, 4, CParser::RuleFunctionDecl);
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
    setState(40);
    match(CParser::INT_KW);
    setState(41);
    match(CParser::IDENT);
    setState(42);
    match(CParser::LPAREN);
    setState(44);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == CParser::INT_KW) {
      setState(43);
      paramList();
    }
    setState(46);
    match(CParser::RPAREN);
    setState(49);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == CParser::AT) {
      setState(47);
      match(CParser::AT);
      setState(48);
      antlrcpp::downCast<FunctionDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
    }
    setState(53);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case CParser::LBRACE: {
        setState(51);
        block();
        break;
      }

      case CParser::SEMI: {
        setState(52);
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
  enterRule(_localctx, 6, CParser::RuleParamList);
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
    setState(55);
    param();
    setState(60);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(56);
      match(CParser::COMMA);
      setState(57);
      param();
      setState(62);
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

tree::TerminalNode* CParser::ParamContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
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
  enterRule(_localctx, 8, CParser::RuleParam);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(63);
    match(CParser::INT_KW);
    setState(64);
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

//----------------- GlobalArrayDeclContext ------------------------------------------------------------------

tree::TerminalNode* CParser::GlobalArrayDeclContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
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
//----------------- GlobalScalarDeclContext ------------------------------------------------------------------

tree::TerminalNode* CParser::GlobalScalarDeclContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
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
  enterRule(_localctx, 10, CParser::RuleGlobalDecl);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(94);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 11, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::GlobalArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(66);
      match(CParser::INT_KW);
      setState(67);
      match(CParser::IDENT);
      setState(68);
      match(CParser::LBRACK);
      setState(69);
      antlrcpp::downCast<GlobalArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(70);
      match(CParser::RBRACK);
      setState(73);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(71);
        match(CParser::ASSIGN);
        setState(72);
        arrayInit();
      }
      setState(77);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::AT) {
        setState(75);
        match(CParser::AT);
        setState(76);
        antlrcpp::downCast<GlobalArrayDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
      }
      setState(79);
      match(CParser::SEMI);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::GlobalScalarDeclContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(80);
      match(CParser::INT_KW);
      setState(81);
      match(CParser::IDENT);
      setState(87);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(82);
        match(CParser::ASSIGN);
        setState(84);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::MINUS) {
          setState(83);
          antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->sign = match(CParser::MINUS);
        }
        setState(86);
        antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->val = match(CParser::NUMBER);
      }
      setState(91);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::AT) {
        setState(89);
        match(CParser::AT);
        setState(90);
        antlrcpp::downCast<GlobalScalarDeclContext *>(_localctx)->addr = match(CParser::NUMBER);
      }
      setState(93);
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
  enterRule(_localctx, 12, CParser::RuleArrayInit);
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
    match(CParser::LBRACE);
    setState(97);
    antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext = arrayInitElem();
    antlrcpp::downCast<ArrayInitContext *>(_localctx)->elem.push_back(antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext);
    setState(102);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(98);
      match(CParser::COMMA);
      setState(99);
      antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext = arrayInitElem();
      antlrcpp::downCast<ArrayInitContext *>(_localctx)->elem.push_back(antlrcpp::downCast<ArrayInitContext *>(_localctx)->arrayInitElemContext);
      setState(104);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(105);
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

CParser::NumArrayElemContext::NumArrayElemContext(ArrayInitElemContext *ctx) { copyFrom(ctx); }


std::any CParser::NumArrayElemContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<CParserVisitor*>(visitor))
    return parserVisitor->visitNumArrayElem(this);
  else
    return visitor->visitChildren(this);
}
CParser::ArrayInitElemContext* CParser::arrayInitElem() {
  ArrayInitElemContext *_localctx = _tracker.createInstance<ArrayInitElemContext>(_ctx, getState());
  enterRule(_localctx, 14, CParser::RuleArrayInitElem);

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
      case CParser::NUMBER: {
        _localctx = _tracker.createInstance<CParser::NumArrayElemContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(107);
        match(CParser::NUMBER);
        break;
      }

      case CParser::IDENT: {
        _localctx = _tracker.createInstance<CParser::SymArrayElemContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(108);
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
  enterRule(_localctx, 16, CParser::RuleBlock);
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
    match(CParser::LBRACE);
    setState(115);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 9758169778422) != 0)) {
      setState(112);
      statement();
      setState(117);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(118);
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
CParser::StatementContext* CParser::statement() {
  StatementContext *_localctx = _tracker.createInstance<StatementContext>(_ctx, getState());
  enterRule(_localctx, 18, CParser::RuleStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(165);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case CParser::LBRACE: {
        _localctx = _tracker.createInstance<CParser::BlockStmtContext>(_localctx);
        enterOuterAlt(_localctx, 1);
        setState(120);
        block();
        break;
      }

      case CParser::IF_KW: {
        _localctx = _tracker.createInstance<CParser::IfStmtContext>(_localctx);
        enterOuterAlt(_localctx, 2);
        setState(121);
        match(CParser::IF_KW);
        setState(122);
        match(CParser::LPAREN);
        setState(123);
        expr();
        setState(124);
        match(CParser::RPAREN);
        setState(125);
        antlrcpp::downCast<IfStmtContext *>(_localctx)->thenS = statement();
        setState(128);
        _errHandler->sync(this);

        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 15, _ctx)) {
        case 1: {
          setState(126);
          match(CParser::ELSE_KW);
          setState(127);
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
        setState(130);
        match(CParser::WHILE_KW);
        setState(131);
        match(CParser::LPAREN);
        setState(132);
        expr();
        setState(133);
        match(CParser::RPAREN);
        setState(134);
        statement();
        break;
      }

      case CParser::FOR_KW: {
        _localctx = _tracker.createInstance<CParser::ForStmtContext>(_localctx);
        enterOuterAlt(_localctx, 4);
        setState(136);
        match(CParser::FOR_KW);
        setState(137);
        match(CParser::LPAREN);
        setState(139);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 962076737666) != 0)) {
          setState(138);
          antlrcpp::downCast<ForStmtContext *>(_localctx)->init = simpleStmt();
        }
        setState(141);
        match(CParser::SEMI);
        setState(143);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 962076737664) != 0)) {
          setState(142);
          antlrcpp::downCast<ForStmtContext *>(_localctx)->cond = expr();
        }
        setState(145);
        match(CParser::SEMI);
        setState(147);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 962076737666) != 0)) {
          setState(146);
          antlrcpp::downCast<ForStmtContext *>(_localctx)->post = simpleStmt();
        }
        setState(149);
        match(CParser::RPAREN);
        setState(150);
        statement();
        break;
      }

      case CParser::RETURN_KW: {
        _localctx = _tracker.createInstance<CParser::ReturnStmtContext>(_localctx);
        enterOuterAlt(_localctx, 5);
        setState(151);
        match(CParser::RETURN_KW);
        setState(153);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 962076737664) != 0)) {
          setState(152);
          expr();
        }
        setState(155);
        match(CParser::SEMI);
        break;
      }

      case CParser::ASM_BEGIN: {
        _localctx = _tracker.createInstance<CParser::AsmStmtContext>(_localctx);
        enterOuterAlt(_localctx, 6);
        setState(156);
        match(CParser::ASM_BEGIN);
        setState(158);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == CParser::ASM_TEXT) {
          setState(157);
          antlrcpp::downCast<AsmStmtContext *>(_localctx)->text = match(CParser::ASM_TEXT);
        }
        setState(160);
        match(CParser::ASM_END);
        break;
      }

      case CParser::INT_KW:
      case CParser::LPAREN:
      case CParser::STAR:
      case CParser::AMP:
      case CParser::MINUS:
      case CParser::BANG:
      case CParser::TILDE:
      case CParser::IDENT:
      case CParser::NUMBER:
      case CParser::CHARLIT: {
        _localctx = _tracker.createInstance<CParser::SimpleStmtWrapperContext>(_localctx);
        enterOuterAlt(_localctx, 7);
        setState(161);
        simpleStmt();
        setState(162);
        match(CParser::SEMI);
        break;
      }

      case CParser::SEMI: {
        _localctx = _tracker.createInstance<CParser::EmptyStmtContext>(_localctx);
        enterOuterAlt(_localctx, 8);
        setState(164);
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

tree::TerminalNode* CParser::LocalDeclContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
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

tree::TerminalNode* CParser::LocalArrayDeclContext::INT_KW() {
  return getToken(CParser::INT_KW, 0);
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
  enterRule(_localctx, 20, CParser::RuleSimpleStmt);
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
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 24, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::LocalArrayDeclContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(167);
      match(CParser::INT_KW);
      setState(168);
      match(CParser::IDENT);
      setState(169);
      match(CParser::LBRACK);
      setState(170);
      antlrcpp::downCast<LocalArrayDeclContext *>(_localctx)->size = match(CParser::NUMBER);
      setState(171);
      match(CParser::RBRACK);
      setState(174);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(172);
        match(CParser::ASSIGN);
        setState(173);
        arrayInit();
      }
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::LocalDeclContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(176);
      match(CParser::INT_KW);
      setState(177);
      match(CParser::IDENT);
      setState(180);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == CParser::ASSIGN) {
        setState(178);
        match(CParser::ASSIGN);
        setState(179);
        expr();
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::ExprStmtContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(182);
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

tree::TerminalNode* CParser::AssignIndexExprContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

std::vector<CParser::ExprContext *> CParser::AssignIndexExprContext::expr() {
  return getRuleContexts<CParser::ExprContext>();
}

CParser::ExprContext* CParser::AssignIndexExprContext::expr(size_t i) {
  return getRuleContext<CParser::ExprContext>(i);
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

tree::TerminalNode* CParser::AssignDerefExprContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
}

CParser::CondExprContext* CParser::AssignDerefExprContext::condExpr() {
  return getRuleContext<CParser::CondExprContext>(0);
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

tree::TerminalNode* CParser::AssignExprContext::ASSIGN() {
  return getToken(CParser::ASSIGN, 0);
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
  enterRule(_localctx, 22, CParser::RuleExpr);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(201);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 25, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CParser::AssignDerefExprContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(185);
      match(CParser::STAR);
      setState(186);
      antlrcpp::downCast<AssignDerefExprContext *>(_localctx)->ptr = condExpr(0);
      setState(187);
      match(CParser::ASSIGN);
      setState(188);
      antlrcpp::downCast<AssignDerefExprContext *>(_localctx)->val = expr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CParser::AssignIndexExprContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(190);
      match(CParser::IDENT);
      setState(191);
      match(CParser::LBRACK);
      setState(192);
      antlrcpp::downCast<AssignIndexExprContext *>(_localctx)->idx = expr();
      setState(193);
      match(CParser::RBRACK);
      setState(194);
      match(CParser::ASSIGN);
      setState(195);
      antlrcpp::downCast<AssignIndexExprContext *>(_localctx)->val = expr();
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<CParser::AssignExprContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(197);
      match(CParser::IDENT);
      setState(198);
      match(CParser::ASSIGN);
      setState(199);
      expr();
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<CParser::ExprCondContext>(_localctx);
      enterOuterAlt(_localctx, 4);
      setState(200);
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
  size_t startState = 24;
  enterRecursionRule(_localctx, 24, CParser::RuleCondExpr, precedence);

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
    setState(237);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 28, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<CallExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;

      setState(204);
      match(CParser::IDENT);
      setState(205);
      match(CParser::LPAREN);
      setState(207);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 962076737664) != 0)) {
        setState(206);
        argList();
      }
      setState(209);
      match(CParser::RPAREN);
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<CallPtrExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(210);
      match(CParser::LPAREN);
      setState(211);
      expr();
      setState(212);
      match(CParser::RPAREN);
      setState(213);
      match(CParser::LPAREN);
      setState(215);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 962076737664) != 0)) {
        setState(214);
        argList();
      }
      setState(217);
      match(CParser::RPAREN);
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<IndexExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(219);
      match(CParser::IDENT);
      setState(220);
      match(CParser::LBRACK);
      setState(221);
      expr();
      setState(222);
      match(CParser::RBRACK);
      break;
    }

    case 4: {
      _localctx = _tracker.createInstance<VarExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(224);
      match(CParser::IDENT);
      break;
    }

    case 5: {
      _localctx = _tracker.createInstance<NumExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(225);
      match(CParser::NUMBER);
      break;
    }

    case 6: {
      _localctx = _tracker.createInstance<CharExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(226);
      match(CParser::CHARLIT);
      break;
    }

    case 7: {
      _localctx = _tracker.createInstance<ParenExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(227);
      match(CParser::LPAREN);
      setState(228);
      expr();
      setState(229);
      match(CParser::RPAREN);
      break;
    }

    case 8: {
      _localctx = _tracker.createInstance<DerefExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(231);
      match(CParser::STAR);
      setState(232);
      condExpr(13);
      break;
    }

    case 9: {
      _localctx = _tracker.createInstance<AddrOfExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(233);
      match(CParser::AMP);
      setState(234);
      condExpr(12);
      break;
    }

    case 10: {
      _localctx = _tracker.createInstance<UnaryExprContext>(_localctx);
      _ctx = _localctx;
      previousContext = _localctx;
      setState(235);
      antlrcpp::downCast<UnaryExprContext *>(_localctx)->op = _input->LT(1);
      _la = _input->LA(1);
      if (!((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 3670016) != 0))) {
        antlrcpp::downCast<UnaryExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(236);
      condExpr(11);
      break;
    }

    default:
      break;
    }
    _ctx->stop = _input->LT(-1);
    setState(271);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 30, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        if (!_parseListeners.empty())
          triggerExitRuleEvent();
        previousContext = _localctx;
        setState(269);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 29, _ctx)) {
        case 1: {
          auto newContext = _tracker.createInstance<MulExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(239);

          if (!(precpred(_ctx, 10))) throw FailedPredicateException(this, "precpred(_ctx, 10)");
          setState(240);
          antlrcpp::downCast<MulExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!((((_la & ~ 0x3fULL) == 0) &&
            ((1ULL << _la) & 12713984) != 0))) {
            antlrcpp::downCast<MulExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(241);
          condExpr(11);
          break;
        }

        case 2: {
          auto newContext = _tracker.createInstance<AddExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(242);

          if (!(precpred(_ctx, 9))) throw FailedPredicateException(this, "precpred(_ctx, 9)");
          setState(243);
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
          setState(244);
          condExpr(10);
          break;
        }

        case 3: {
          auto newContext = _tracker.createInstance<ShiftExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(245);

          if (!(precpred(_ctx, 8))) throw FailedPredicateException(this, "precpred(_ctx, 8)");
          setState(246);
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
          setState(247);
          condExpr(9);
          break;
        }

        case 4: {
          auto newContext = _tracker.createInstance<RelExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(248);

          if (!(precpred(_ctx, 7))) throw FailedPredicateException(this, "precpred(_ctx, 7)");
          setState(249);
          antlrcpp::downCast<RelExprContext *>(_localctx)->op = _input->LT(1);
          _la = _input->LA(1);
          if (!((((_la & ~ 0x3fULL) == 0) &&
            ((1ULL << _la) & 2013265920) != 0))) {
            antlrcpp::downCast<RelExprContext *>(_localctx)->op = _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(250);
          condExpr(8);
          break;
        }

        case 5: {
          auto newContext = _tracker.createInstance<EqExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(251);

          if (!(precpred(_ctx, 6))) throw FailedPredicateException(this, "precpred(_ctx, 6)");
          setState(252);
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
          setState(253);
          condExpr(7);
          break;
        }

        case 6: {
          auto newContext = _tracker.createInstance<BitAndExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(254);

          if (!(precpred(_ctx, 5))) throw FailedPredicateException(this, "precpred(_ctx, 5)");
          setState(255);
          match(CParser::AMP);
          setState(256);
          condExpr(6);
          break;
        }

        case 7: {
          auto newContext = _tracker.createInstance<BitXorExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(257);

          if (!(precpred(_ctx, 4))) throw FailedPredicateException(this, "precpred(_ctx, 4)");
          setState(258);
          match(CParser::CARET);
          setState(259);
          condExpr(5);
          break;
        }

        case 8: {
          auto newContext = _tracker.createInstance<BitOrExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(260);

          if (!(precpred(_ctx, 3))) throw FailedPredicateException(this, "precpred(_ctx, 3)");
          setState(261);
          match(CParser::PIPE);
          setState(262);
          condExpr(4);
          break;
        }

        case 9: {
          auto newContext = _tracker.createInstance<AndExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(263);

          if (!(precpred(_ctx, 2))) throw FailedPredicateException(this, "precpred(_ctx, 2)");
          setState(264);
          match(CParser::ANDAND);
          setState(265);
          condExpr(3);
          break;
        }

        case 10: {
          auto newContext = _tracker.createInstance<OrExprContext>(_tracker.createInstance<CondExprContext>(parentContext, parentState));
          _localctx = newContext;
          pushNewRecursionContext(newContext, startState, RuleCondExpr);
          setState(266);

          if (!(precpred(_ctx, 1))) throw FailedPredicateException(this, "precpred(_ctx, 1)");
          setState(267);
          match(CParser::OROR);
          setState(268);
          condExpr(2);
          break;
        }

        default:
          break;
        } 
      }
      setState(273);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 30, _ctx);
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
  enterRule(_localctx, 26, CParser::RuleArgList);
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
    setState(274);
    expr();
    setState(279);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == CParser::COMMA) {
      setState(275);
      match(CParser::COMMA);
      setState(276);
      expr();
      setState(281);
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
    case 12: return condExprSempred(antlrcpp::downCast<CondExprContext *>(context), predicateIndex);

  default:
    break;
  }
  return true;
}

bool CParser::condExprSempred(CondExprContext *_localctx, size_t predicateIndex) {
  switch (predicateIndex) {
    case 0: return precpred(_ctx, 10);
    case 1: return precpred(_ctx, 9);
    case 2: return precpred(_ctx, 8);
    case 3: return precpred(_ctx, 7);
    case 4: return precpred(_ctx, 6);
    case 5: return precpred(_ctx, 5);
    case 6: return precpred(_ctx, 4);
    case 7: return precpred(_ctx, 3);
    case 8: return precpred(_ctx, 2);
    case 9: return precpred(_ctx, 1);

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
