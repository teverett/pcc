
// Generated from /Users/tom/projects/pcc/grammar/RccAsm.g4 by ANTLR 4.13.2


#include "RccAsmVisitor.h"

#include "RccAsmParser.h"


using namespace antlrcpp;

using namespace antlr4;

namespace {

struct RccAsmParserStaticData final {
  RccAsmParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  RccAsmParserStaticData(const RccAsmParserStaticData&) = delete;
  RccAsmParserStaticData(RccAsmParserStaticData&&) = delete;
  RccAsmParserStaticData& operator=(const RccAsmParserStaticData&) = delete;
  RccAsmParserStaticData& operator=(RccAsmParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag rccasmParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<RccAsmParserStaticData> rccasmParserStaticData = nullptr;

void rccasmParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (rccasmParserStaticData != nullptr) {
    return;
  }
#else
  assert(rccasmParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<RccAsmParserStaticData>(
    std::vector<std::string>{
      "program", "line", "label", "statement", "operand", "expr", "syllable", 
      "sign", "atom"
    },
    std::vector<std::string>{
      "", "'times'", "", "", "':'", "'='", "','", "'+'", "'-'", "'['", "']'", 
      "'('", "')'"
    },
    std::vector<std::string>{
      "", "", "IDENT", "NUMBER", "COLON", "EQUALS", "COMMA", "PLUS", "MINUS", 
      "LBRACKET", "RBRACKET", "LPAREN", "RPAREN", "COMMENT", "NEWLINE", 
      "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,15,104,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,1,0,5,0,20,8,0,10,0,12,0,23,9,0,1,0,1,0,1,1,5,1,28,8,1,
  	10,1,12,1,31,9,1,1,1,3,1,34,8,1,1,1,1,1,1,2,1,2,1,2,1,3,1,3,1,3,1,3,1,
  	3,1,3,1,3,1,3,1,3,1,3,1,3,5,3,52,8,3,10,3,12,3,55,9,3,3,3,57,8,3,1,3,
  	1,3,1,3,1,3,5,3,63,8,3,10,3,12,3,66,9,3,3,3,68,8,3,3,3,70,8,3,1,4,3,4,
  	73,8,4,1,4,1,4,1,4,1,4,1,4,3,4,80,8,4,1,5,1,5,5,5,84,8,5,10,5,12,5,87,
  	9,5,1,6,3,6,90,8,6,1,6,1,6,1,7,1,7,1,8,1,8,1,8,1,8,1,8,1,8,3,8,102,8,
  	8,1,8,0,0,9,0,2,4,6,8,10,12,14,16,0,2,1,0,2,3,1,0,7,8,109,0,21,1,0,0,
  	0,2,29,1,0,0,0,4,37,1,0,0,0,6,69,1,0,0,0,8,79,1,0,0,0,10,81,1,0,0,0,12,
  	89,1,0,0,0,14,93,1,0,0,0,16,101,1,0,0,0,18,20,3,2,1,0,19,18,1,0,0,0,20,
  	23,1,0,0,0,21,19,1,0,0,0,21,22,1,0,0,0,22,24,1,0,0,0,23,21,1,0,0,0,24,
  	25,5,0,0,1,25,1,1,0,0,0,26,28,3,4,2,0,27,26,1,0,0,0,28,31,1,0,0,0,29,
  	27,1,0,0,0,29,30,1,0,0,0,30,33,1,0,0,0,31,29,1,0,0,0,32,34,3,6,3,0,33,
  	32,1,0,0,0,33,34,1,0,0,0,34,35,1,0,0,0,35,36,5,14,0,0,36,3,1,0,0,0,37,
  	38,5,2,0,0,38,39,5,4,0,0,39,5,1,0,0,0,40,41,5,2,0,0,41,42,5,5,0,0,42,
  	70,3,10,5,0,43,44,5,1,0,0,44,45,5,11,0,0,45,46,3,10,5,0,46,47,5,12,0,
  	0,47,56,7,0,0,0,48,53,3,8,4,0,49,50,5,6,0,0,50,52,3,8,4,0,51,49,1,0,0,
  	0,52,55,1,0,0,0,53,51,1,0,0,0,53,54,1,0,0,0,54,57,1,0,0,0,55,53,1,0,0,
  	0,56,48,1,0,0,0,56,57,1,0,0,0,57,70,1,0,0,0,58,67,7,0,0,0,59,64,3,8,4,
  	0,60,61,5,6,0,0,61,63,3,8,4,0,62,60,1,0,0,0,63,66,1,0,0,0,64,62,1,0,0,
  	0,64,65,1,0,0,0,65,68,1,0,0,0,66,64,1,0,0,0,67,59,1,0,0,0,67,68,1,0,0,
  	0,68,70,1,0,0,0,69,40,1,0,0,0,69,43,1,0,0,0,69,58,1,0,0,0,70,7,1,0,0,
  	0,71,73,5,2,0,0,72,71,1,0,0,0,72,73,1,0,0,0,73,74,1,0,0,0,74,75,5,9,0,
  	0,75,76,3,10,5,0,76,77,5,10,0,0,77,80,1,0,0,0,78,80,3,10,5,0,79,72,1,
  	0,0,0,79,78,1,0,0,0,80,9,1,0,0,0,81,85,3,12,6,0,82,84,3,12,6,0,83,82,
  	1,0,0,0,84,87,1,0,0,0,85,83,1,0,0,0,85,86,1,0,0,0,86,11,1,0,0,0,87,85,
  	1,0,0,0,88,90,3,14,7,0,89,88,1,0,0,0,89,90,1,0,0,0,90,91,1,0,0,0,91,92,
  	3,16,8,0,92,13,1,0,0,0,93,94,7,1,0,0,94,15,1,0,0,0,95,102,5,2,0,0,96,
  	102,5,3,0,0,97,98,5,11,0,0,98,99,3,10,5,0,99,100,5,12,0,0,100,102,1,0,
  	0,0,101,95,1,0,0,0,101,96,1,0,0,0,101,97,1,0,0,0,102,17,1,0,0,0,13,21,
  	29,33,53,56,64,67,69,72,79,85,89,101
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  rccasmParserStaticData = std::move(staticData);
}

}

RccAsmParser::RccAsmParser(TokenStream *input) : RccAsmParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

RccAsmParser::RccAsmParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  RccAsmParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *rccasmParserStaticData->atn, rccasmParserStaticData->decisionToDFA, rccasmParserStaticData->sharedContextCache, options);
}

RccAsmParser::~RccAsmParser() {
  delete _interpreter;
}

const atn::ATN& RccAsmParser::getATN() const {
  return *rccasmParserStaticData->atn;
}

std::string RccAsmParser::getGrammarFileName() const {
  return "RccAsm.g4";
}

const std::vector<std::string>& RccAsmParser::getRuleNames() const {
  return rccasmParserStaticData->ruleNames;
}

const dfa::Vocabulary& RccAsmParser::getVocabulary() const {
  return rccasmParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView RccAsmParser::getSerializedATN() const {
  return rccasmParserStaticData->serializedATN;
}


//----------------- ProgramContext ------------------------------------------------------------------

RccAsmParser::ProgramContext::ProgramContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccAsmParser::ProgramContext::EOF() {
  return getToken(RccAsmParser::EOF, 0);
}

std::vector<RccAsmParser::LineContext *> RccAsmParser::ProgramContext::line() {
  return getRuleContexts<RccAsmParser::LineContext>();
}

RccAsmParser::LineContext* RccAsmParser::ProgramContext::line(size_t i) {
  return getRuleContext<RccAsmParser::LineContext>(i);
}


size_t RccAsmParser::ProgramContext::getRuleIndex() const {
  return RccAsmParser::RuleProgram;
}


std::any RccAsmParser::ProgramContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitProgram(this);
  else
    return visitor->visitChildren(this);
}

RccAsmParser::ProgramContext* RccAsmParser::program() {
  ProgramContext *_localctx = _tracker.createInstance<ProgramContext>(_ctx, getState());
  enterRule(_localctx, 0, RccAsmParser::RuleProgram);
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
    setState(21);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 16398) != 0)) {
      setState(18);
      line();
      setState(23);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }
    setState(24);
    match(RccAsmParser::EOF);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LineContext ------------------------------------------------------------------

RccAsmParser::LineContext::LineContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccAsmParser::LineContext::NEWLINE() {
  return getToken(RccAsmParser::NEWLINE, 0);
}

std::vector<RccAsmParser::LabelContext *> RccAsmParser::LineContext::label() {
  return getRuleContexts<RccAsmParser::LabelContext>();
}

RccAsmParser::LabelContext* RccAsmParser::LineContext::label(size_t i) {
  return getRuleContext<RccAsmParser::LabelContext>(i);
}

RccAsmParser::StatementContext* RccAsmParser::LineContext::statement() {
  return getRuleContext<RccAsmParser::StatementContext>(0);
}


size_t RccAsmParser::LineContext::getRuleIndex() const {
  return RccAsmParser::RuleLine;
}


std::any RccAsmParser::LineContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitLine(this);
  else
    return visitor->visitChildren(this);
}

RccAsmParser::LineContext* RccAsmParser::line() {
  LineContext *_localctx = _tracker.createInstance<LineContext>(_ctx, getState());
  enterRule(_localctx, 2, RccAsmParser::RuleLine);
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
    setState(29);
    _errHandler->sync(this);
    alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 1, _ctx);
    while (alt != 2 && alt != atn::ATN::INVALID_ALT_NUMBER) {
      if (alt == 1) {
        setState(26);
        label(); 
      }
      setState(31);
      _errHandler->sync(this);
      alt = getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 1, _ctx);
    }
    setState(33);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 14) != 0)) {
      setState(32);
      statement();
    }
    setState(35);
    match(RccAsmParser::NEWLINE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LabelContext ------------------------------------------------------------------

RccAsmParser::LabelContext::LabelContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccAsmParser::LabelContext::IDENT() {
  return getToken(RccAsmParser::IDENT, 0);
}

tree::TerminalNode* RccAsmParser::LabelContext::COLON() {
  return getToken(RccAsmParser::COLON, 0);
}


size_t RccAsmParser::LabelContext::getRuleIndex() const {
  return RccAsmParser::RuleLabel;
}


std::any RccAsmParser::LabelContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitLabel(this);
  else
    return visitor->visitChildren(this);
}

RccAsmParser::LabelContext* RccAsmParser::label() {
  LabelContext *_localctx = _tracker.createInstance<LabelContext>(_ctx, getState());
  enterRule(_localctx, 4, RccAsmParser::RuleLabel);

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
    match(RccAsmParser::IDENT);
    setState(38);
    match(RccAsmParser::COLON);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StatementContext ------------------------------------------------------------------

RccAsmParser::StatementContext::StatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t RccAsmParser::StatementContext::getRuleIndex() const {
  return RccAsmParser::RuleStatement;
}

void RccAsmParser::StatementContext::copyFrom(StatementContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- TimesStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccAsmParser::TimesStmtContext::LPAREN() {
  return getToken(RccAsmParser::LPAREN, 0);
}

tree::TerminalNode* RccAsmParser::TimesStmtContext::RPAREN() {
  return getToken(RccAsmParser::RPAREN, 0);
}

RccAsmParser::ExprContext* RccAsmParser::TimesStmtContext::expr() {
  return getRuleContext<RccAsmParser::ExprContext>(0);
}

tree::TerminalNode* RccAsmParser::TimesStmtContext::IDENT() {
  return getToken(RccAsmParser::IDENT, 0);
}

tree::TerminalNode* RccAsmParser::TimesStmtContext::NUMBER() {
  return getToken(RccAsmParser::NUMBER, 0);
}

std::vector<RccAsmParser::OperandContext *> RccAsmParser::TimesStmtContext::operand() {
  return getRuleContexts<RccAsmParser::OperandContext>();
}

RccAsmParser::OperandContext* RccAsmParser::TimesStmtContext::operand(size_t i) {
  return getRuleContext<RccAsmParser::OperandContext>(i);
}

std::vector<tree::TerminalNode *> RccAsmParser::TimesStmtContext::COMMA() {
  return getTokens(RccAsmParser::COMMA);
}

tree::TerminalNode* RccAsmParser::TimesStmtContext::COMMA(size_t i) {
  return getToken(RccAsmParser::COMMA, i);
}

RccAsmParser::TimesStmtContext::TimesStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any RccAsmParser::TimesStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitTimesStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AssignStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccAsmParser::AssignStmtContext::EQUALS() {
  return getToken(RccAsmParser::EQUALS, 0);
}

tree::TerminalNode* RccAsmParser::AssignStmtContext::IDENT() {
  return getToken(RccAsmParser::IDENT, 0);
}

RccAsmParser::ExprContext* RccAsmParser::AssignStmtContext::expr() {
  return getRuleContext<RccAsmParser::ExprContext>(0);
}

RccAsmParser::AssignStmtContext::AssignStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any RccAsmParser::AssignStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitAssignStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- InstrStmtContext ------------------------------------------------------------------

tree::TerminalNode* RccAsmParser::InstrStmtContext::IDENT() {
  return getToken(RccAsmParser::IDENT, 0);
}

tree::TerminalNode* RccAsmParser::InstrStmtContext::NUMBER() {
  return getToken(RccAsmParser::NUMBER, 0);
}

std::vector<RccAsmParser::OperandContext *> RccAsmParser::InstrStmtContext::operand() {
  return getRuleContexts<RccAsmParser::OperandContext>();
}

RccAsmParser::OperandContext* RccAsmParser::InstrStmtContext::operand(size_t i) {
  return getRuleContext<RccAsmParser::OperandContext>(i);
}

std::vector<tree::TerminalNode *> RccAsmParser::InstrStmtContext::COMMA() {
  return getTokens(RccAsmParser::COMMA);
}

tree::TerminalNode* RccAsmParser::InstrStmtContext::COMMA(size_t i) {
  return getToken(RccAsmParser::COMMA, i);
}

RccAsmParser::InstrStmtContext::InstrStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any RccAsmParser::InstrStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitInstrStmt(this);
  else
    return visitor->visitChildren(this);
}
RccAsmParser::StatementContext* RccAsmParser::statement() {
  StatementContext *_localctx = _tracker.createInstance<StatementContext>(_ctx, getState());
  enterRule(_localctx, 6, RccAsmParser::RuleStatement);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(69);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 7, _ctx)) {
    case 1: {
      _localctx = _tracker.createInstance<RccAsmParser::AssignStmtContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(40);
      antlrcpp::downCast<AssignStmtContext *>(_localctx)->lhs = match(RccAsmParser::IDENT);
      setState(41);
      match(RccAsmParser::EQUALS);
      setState(42);
      antlrcpp::downCast<AssignStmtContext *>(_localctx)->rhs = expr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<RccAsmParser::TimesStmtContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(43);
      match(RccAsmParser::T__0);
      setState(44);
      match(RccAsmParser::LPAREN);
      setState(45);
      antlrcpp::downCast<TimesStmtContext *>(_localctx)->count = expr();
      setState(46);
      match(RccAsmParser::RPAREN);
      setState(47);
      antlrcpp::downCast<TimesStmtContext *>(_localctx)->mnemonic = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == RccAsmParser::IDENT

      || _la == RccAsmParser::NUMBER)) {
        antlrcpp::downCast<TimesStmtContext *>(_localctx)->mnemonic = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(56);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 2956) != 0)) {
        setState(48);
        operand();
        setState(53);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == RccAsmParser::COMMA) {
          setState(49);
          match(RccAsmParser::COMMA);
          setState(50);
          operand();
          setState(55);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
      }
      break;
    }

    case 3: {
      _localctx = _tracker.createInstance<RccAsmParser::InstrStmtContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(58);
      antlrcpp::downCast<InstrStmtContext *>(_localctx)->mnemonic = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == RccAsmParser::IDENT

      || _la == RccAsmParser::NUMBER)) {
        antlrcpp::downCast<InstrStmtContext *>(_localctx)->mnemonic = _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(67);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 2956) != 0)) {
        setState(59);
        operand();
        setState(64);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == RccAsmParser::COMMA) {
          setState(60);
          match(RccAsmParser::COMMA);
          setState(61);
          operand();
          setState(66);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
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

//----------------- OperandContext ------------------------------------------------------------------

RccAsmParser::OperandContext::OperandContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccAsmParser::OperandContext::LBRACKET() {
  return getToken(RccAsmParser::LBRACKET, 0);
}

RccAsmParser::ExprContext* RccAsmParser::OperandContext::expr() {
  return getRuleContext<RccAsmParser::ExprContext>(0);
}

tree::TerminalNode* RccAsmParser::OperandContext::RBRACKET() {
  return getToken(RccAsmParser::RBRACKET, 0);
}

tree::TerminalNode* RccAsmParser::OperandContext::IDENT() {
  return getToken(RccAsmParser::IDENT, 0);
}


size_t RccAsmParser::OperandContext::getRuleIndex() const {
  return RccAsmParser::RuleOperand;
}


std::any RccAsmParser::OperandContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitOperand(this);
  else
    return visitor->visitChildren(this);
}

RccAsmParser::OperandContext* RccAsmParser::operand() {
  OperandContext *_localctx = _tracker.createInstance<OperandContext>(_ctx, getState());
  enterRule(_localctx, 8, RccAsmParser::RuleOperand);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(79);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 9, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(72);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == RccAsmParser::IDENT) {
        setState(71);
        antlrcpp::downCast<OperandContext *>(_localctx)->size = match(RccAsmParser::IDENT);
      }
      setState(74);
      match(RccAsmParser::LBRACKET);
      setState(75);
      expr();
      setState(76);
      match(RccAsmParser::RBRACKET);
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(78);
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

RccAsmParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<RccAsmParser::SyllableContext *> RccAsmParser::ExprContext::syllable() {
  return getRuleContexts<RccAsmParser::SyllableContext>();
}

RccAsmParser::SyllableContext* RccAsmParser::ExprContext::syllable(size_t i) {
  return getRuleContext<RccAsmParser::SyllableContext>(i);
}


size_t RccAsmParser::ExprContext::getRuleIndex() const {
  return RccAsmParser::RuleExpr;
}


std::any RccAsmParser::ExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitExpr(this);
  else
    return visitor->visitChildren(this);
}

RccAsmParser::ExprContext* RccAsmParser::expr() {
  ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, getState());
  enterRule(_localctx, 10, RccAsmParser::RuleExpr);
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
    setState(81);
    syllable();
    setState(85);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2444) != 0)) {
      setState(82);
      syllable();
      setState(87);
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

//----------------- SyllableContext ------------------------------------------------------------------

RccAsmParser::SyllableContext::SyllableContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

RccAsmParser::AtomContext* RccAsmParser::SyllableContext::atom() {
  return getRuleContext<RccAsmParser::AtomContext>(0);
}

RccAsmParser::SignContext* RccAsmParser::SyllableContext::sign() {
  return getRuleContext<RccAsmParser::SignContext>(0);
}


size_t RccAsmParser::SyllableContext::getRuleIndex() const {
  return RccAsmParser::RuleSyllable;
}


std::any RccAsmParser::SyllableContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitSyllable(this);
  else
    return visitor->visitChildren(this);
}

RccAsmParser::SyllableContext* RccAsmParser::syllable() {
  SyllableContext *_localctx = _tracker.createInstance<SyllableContext>(_ctx, getState());
  enterRule(_localctx, 12, RccAsmParser::RuleSyllable);
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
    setState(89);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == RccAsmParser::PLUS

    || _la == RccAsmParser::MINUS) {
      setState(88);
      sign();
    }
    setState(91);
    atom();
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SignContext ------------------------------------------------------------------

RccAsmParser::SignContext::SignContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccAsmParser::SignContext::PLUS() {
  return getToken(RccAsmParser::PLUS, 0);
}

tree::TerminalNode* RccAsmParser::SignContext::MINUS() {
  return getToken(RccAsmParser::MINUS, 0);
}


size_t RccAsmParser::SignContext::getRuleIndex() const {
  return RccAsmParser::RuleSign;
}


std::any RccAsmParser::SignContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitSign(this);
  else
    return visitor->visitChildren(this);
}

RccAsmParser::SignContext* RccAsmParser::sign() {
  SignContext *_localctx = _tracker.createInstance<SignContext>(_ctx, getState());
  enterRule(_localctx, 14, RccAsmParser::RuleSign);
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
    setState(93);
    _la = _input->LA(1);
    if (!(_la == RccAsmParser::PLUS

    || _la == RccAsmParser::MINUS)) {
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

//----------------- AtomContext ------------------------------------------------------------------

RccAsmParser::AtomContext::AtomContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* RccAsmParser::AtomContext::IDENT() {
  return getToken(RccAsmParser::IDENT, 0);
}

tree::TerminalNode* RccAsmParser::AtomContext::NUMBER() {
  return getToken(RccAsmParser::NUMBER, 0);
}

tree::TerminalNode* RccAsmParser::AtomContext::LPAREN() {
  return getToken(RccAsmParser::LPAREN, 0);
}

RccAsmParser::ExprContext* RccAsmParser::AtomContext::expr() {
  return getRuleContext<RccAsmParser::ExprContext>(0);
}

tree::TerminalNode* RccAsmParser::AtomContext::RPAREN() {
  return getToken(RccAsmParser::RPAREN, 0);
}


size_t RccAsmParser::AtomContext::getRuleIndex() const {
  return RccAsmParser::RuleAtom;
}


std::any RccAsmParser::AtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<RccAsmVisitor*>(visitor))
    return parserVisitor->visitAtom(this);
  else
    return visitor->visitChildren(this);
}

RccAsmParser::AtomContext* RccAsmParser::atom() {
  AtomContext *_localctx = _tracker.createInstance<AtomContext>(_ctx, getState());
  enterRule(_localctx, 16, RccAsmParser::RuleAtom);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(101);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case RccAsmParser::IDENT: {
        enterOuterAlt(_localctx, 1);
        setState(95);
        match(RccAsmParser::IDENT);
        break;
      }

      case RccAsmParser::NUMBER: {
        enterOuterAlt(_localctx, 2);
        setState(96);
        match(RccAsmParser::NUMBER);
        break;
      }

      case RccAsmParser::LPAREN: {
        enterOuterAlt(_localctx, 3);
        setState(97);
        match(RccAsmParser::LPAREN);
        setState(98);
        expr();
        setState(99);
        match(RccAsmParser::RPAREN);
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

void RccAsmParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  rccasmParserInitialize();
#else
  ::antlr4::internal::call_once(rccasmParserOnceFlag, rccasmParserInitialize);
#endif
}
