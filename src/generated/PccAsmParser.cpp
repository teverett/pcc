
// Generated from /Users/tom/projects/pcc/grammar/PccAsm.g4 by ANTLR 4.13.2


#include "PccAsmVisitor.h"

#include "PccAsmParser.h"


using namespace antlrcpp;

using namespace antlr4;

namespace {

struct PccAsmParserStaticData final {
  PccAsmParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  PccAsmParserStaticData(const PccAsmParserStaticData&) = delete;
  PccAsmParserStaticData(PccAsmParserStaticData&&) = delete;
  PccAsmParserStaticData& operator=(const PccAsmParserStaticData&) = delete;
  PccAsmParserStaticData& operator=(PccAsmParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag pccasmParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<PccAsmParserStaticData> pccasmParserStaticData = nullptr;

void pccasmParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (pccasmParserStaticData != nullptr) {
    return;
  }
#else
  assert(pccasmParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<PccAsmParserStaticData>(
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
  	4,1,15,101,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,1,0,5,0,20,8,0,10,0,12,0,23,9,0,1,0,1,0,1,1,5,1,28,8,1,
  	10,1,12,1,31,9,1,1,1,3,1,34,8,1,1,1,1,1,1,2,1,2,1,2,1,3,1,3,1,3,1,3,1,
  	3,1,3,1,3,1,3,1,3,1,3,1,3,5,3,52,8,3,10,3,12,3,55,9,3,3,3,57,8,3,1,3,
  	1,3,1,3,1,3,5,3,63,8,3,10,3,12,3,66,9,3,3,3,68,8,3,3,3,70,8,3,1,4,1,4,
  	1,4,1,4,1,4,3,4,77,8,4,1,5,1,5,5,5,81,8,5,10,5,12,5,84,9,5,1,6,3,6,87,
  	8,6,1,6,1,6,1,7,1,7,1,8,1,8,1,8,1,8,1,8,1,8,3,8,99,8,8,1,8,0,0,9,0,2,
  	4,6,8,10,12,14,16,0,2,1,0,2,3,1,0,7,8,105,0,21,1,0,0,0,2,29,1,0,0,0,4,
  	37,1,0,0,0,6,69,1,0,0,0,8,76,1,0,0,0,10,78,1,0,0,0,12,86,1,0,0,0,14,90,
  	1,0,0,0,16,98,1,0,0,0,18,20,3,2,1,0,19,18,1,0,0,0,20,23,1,0,0,0,21,19,
  	1,0,0,0,21,22,1,0,0,0,22,24,1,0,0,0,23,21,1,0,0,0,24,25,5,0,0,1,25,1,
  	1,0,0,0,26,28,3,4,2,0,27,26,1,0,0,0,28,31,1,0,0,0,29,27,1,0,0,0,29,30,
  	1,0,0,0,30,33,1,0,0,0,31,29,1,0,0,0,32,34,3,6,3,0,33,32,1,0,0,0,33,34,
  	1,0,0,0,34,35,1,0,0,0,35,36,5,14,0,0,36,3,1,0,0,0,37,38,5,2,0,0,38,39,
  	5,4,0,0,39,5,1,0,0,0,40,41,5,2,0,0,41,42,5,5,0,0,42,70,3,10,5,0,43,44,
  	5,1,0,0,44,45,5,11,0,0,45,46,3,10,5,0,46,47,5,12,0,0,47,56,7,0,0,0,48,
  	53,3,8,4,0,49,50,5,6,0,0,50,52,3,8,4,0,51,49,1,0,0,0,52,55,1,0,0,0,53,
  	51,1,0,0,0,53,54,1,0,0,0,54,57,1,0,0,0,55,53,1,0,0,0,56,48,1,0,0,0,56,
  	57,1,0,0,0,57,70,1,0,0,0,58,67,7,0,0,0,59,64,3,8,4,0,60,61,5,6,0,0,61,
  	63,3,8,4,0,62,60,1,0,0,0,63,66,1,0,0,0,64,62,1,0,0,0,64,65,1,0,0,0,65,
  	68,1,0,0,0,66,64,1,0,0,0,67,59,1,0,0,0,67,68,1,0,0,0,68,70,1,0,0,0,69,
  	40,1,0,0,0,69,43,1,0,0,0,69,58,1,0,0,0,70,7,1,0,0,0,71,72,5,9,0,0,72,
  	73,3,10,5,0,73,74,5,10,0,0,74,77,1,0,0,0,75,77,3,10,5,0,76,71,1,0,0,0,
  	76,75,1,0,0,0,77,9,1,0,0,0,78,82,3,12,6,0,79,81,3,12,6,0,80,79,1,0,0,
  	0,81,84,1,0,0,0,82,80,1,0,0,0,82,83,1,0,0,0,83,11,1,0,0,0,84,82,1,0,0,
  	0,85,87,3,14,7,0,86,85,1,0,0,0,86,87,1,0,0,0,87,88,1,0,0,0,88,89,3,16,
  	8,0,89,13,1,0,0,0,90,91,7,1,0,0,91,15,1,0,0,0,92,99,5,2,0,0,93,99,5,3,
  	0,0,94,95,5,11,0,0,95,96,3,10,5,0,96,97,5,12,0,0,97,99,1,0,0,0,98,92,
  	1,0,0,0,98,93,1,0,0,0,98,94,1,0,0,0,99,17,1,0,0,0,12,21,29,33,53,56,64,
  	67,69,76,82,86,98
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  pccasmParserStaticData = std::move(staticData);
}

}

PccAsmParser::PccAsmParser(TokenStream *input) : PccAsmParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

PccAsmParser::PccAsmParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  PccAsmParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *pccasmParserStaticData->atn, pccasmParserStaticData->decisionToDFA, pccasmParserStaticData->sharedContextCache, options);
}

PccAsmParser::~PccAsmParser() {
  delete _interpreter;
}

const atn::ATN& PccAsmParser::getATN() const {
  return *pccasmParserStaticData->atn;
}

std::string PccAsmParser::getGrammarFileName() const {
  return "PccAsm.g4";
}

const std::vector<std::string>& PccAsmParser::getRuleNames() const {
  return pccasmParserStaticData->ruleNames;
}

const dfa::Vocabulary& PccAsmParser::getVocabulary() const {
  return pccasmParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView PccAsmParser::getSerializedATN() const {
  return pccasmParserStaticData->serializedATN;
}


//----------------- ProgramContext ------------------------------------------------------------------

PccAsmParser::ProgramContext::ProgramContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccAsmParser::ProgramContext::EOF() {
  return getToken(PccAsmParser::EOF, 0);
}

std::vector<PccAsmParser::LineContext *> PccAsmParser::ProgramContext::line() {
  return getRuleContexts<PccAsmParser::LineContext>();
}

PccAsmParser::LineContext* PccAsmParser::ProgramContext::line(size_t i) {
  return getRuleContext<PccAsmParser::LineContext>(i);
}


size_t PccAsmParser::ProgramContext::getRuleIndex() const {
  return PccAsmParser::RuleProgram;
}


std::any PccAsmParser::ProgramContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitProgram(this);
  else
    return visitor->visitChildren(this);
}

PccAsmParser::ProgramContext* PccAsmParser::program() {
  ProgramContext *_localctx = _tracker.createInstance<ProgramContext>(_ctx, getState());
  enterRule(_localctx, 0, PccAsmParser::RuleProgram);
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
    match(PccAsmParser::EOF);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LineContext ------------------------------------------------------------------

PccAsmParser::LineContext::LineContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccAsmParser::LineContext::NEWLINE() {
  return getToken(PccAsmParser::NEWLINE, 0);
}

std::vector<PccAsmParser::LabelContext *> PccAsmParser::LineContext::label() {
  return getRuleContexts<PccAsmParser::LabelContext>();
}

PccAsmParser::LabelContext* PccAsmParser::LineContext::label(size_t i) {
  return getRuleContext<PccAsmParser::LabelContext>(i);
}

PccAsmParser::StatementContext* PccAsmParser::LineContext::statement() {
  return getRuleContext<PccAsmParser::StatementContext>(0);
}


size_t PccAsmParser::LineContext::getRuleIndex() const {
  return PccAsmParser::RuleLine;
}


std::any PccAsmParser::LineContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitLine(this);
  else
    return visitor->visitChildren(this);
}

PccAsmParser::LineContext* PccAsmParser::line() {
  LineContext *_localctx = _tracker.createInstance<LineContext>(_ctx, getState());
  enterRule(_localctx, 2, PccAsmParser::RuleLine);
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
    match(PccAsmParser::NEWLINE);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LabelContext ------------------------------------------------------------------

PccAsmParser::LabelContext::LabelContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccAsmParser::LabelContext::IDENT() {
  return getToken(PccAsmParser::IDENT, 0);
}

tree::TerminalNode* PccAsmParser::LabelContext::COLON() {
  return getToken(PccAsmParser::COLON, 0);
}


size_t PccAsmParser::LabelContext::getRuleIndex() const {
  return PccAsmParser::RuleLabel;
}


std::any PccAsmParser::LabelContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitLabel(this);
  else
    return visitor->visitChildren(this);
}

PccAsmParser::LabelContext* PccAsmParser::label() {
  LabelContext *_localctx = _tracker.createInstance<LabelContext>(_ctx, getState());
  enterRule(_localctx, 4, PccAsmParser::RuleLabel);

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
    match(PccAsmParser::IDENT);
    setState(38);
    match(PccAsmParser::COLON);
   
  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StatementContext ------------------------------------------------------------------

PccAsmParser::StatementContext::StatementContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t PccAsmParser::StatementContext::getRuleIndex() const {
  return PccAsmParser::RuleStatement;
}

void PccAsmParser::StatementContext::copyFrom(StatementContext *ctx) {
  ParserRuleContext::copyFrom(ctx);
}

//----------------- TimesStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccAsmParser::TimesStmtContext::LPAREN() {
  return getToken(PccAsmParser::LPAREN, 0);
}

tree::TerminalNode* PccAsmParser::TimesStmtContext::RPAREN() {
  return getToken(PccAsmParser::RPAREN, 0);
}

PccAsmParser::ExprContext* PccAsmParser::TimesStmtContext::expr() {
  return getRuleContext<PccAsmParser::ExprContext>(0);
}

tree::TerminalNode* PccAsmParser::TimesStmtContext::IDENT() {
  return getToken(PccAsmParser::IDENT, 0);
}

tree::TerminalNode* PccAsmParser::TimesStmtContext::NUMBER() {
  return getToken(PccAsmParser::NUMBER, 0);
}

std::vector<PccAsmParser::OperandContext *> PccAsmParser::TimesStmtContext::operand() {
  return getRuleContexts<PccAsmParser::OperandContext>();
}

PccAsmParser::OperandContext* PccAsmParser::TimesStmtContext::operand(size_t i) {
  return getRuleContext<PccAsmParser::OperandContext>(i);
}

std::vector<tree::TerminalNode *> PccAsmParser::TimesStmtContext::COMMA() {
  return getTokens(PccAsmParser::COMMA);
}

tree::TerminalNode* PccAsmParser::TimesStmtContext::COMMA(size_t i) {
  return getToken(PccAsmParser::COMMA, i);
}

PccAsmParser::TimesStmtContext::TimesStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any PccAsmParser::TimesStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitTimesStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- AssignStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccAsmParser::AssignStmtContext::EQUALS() {
  return getToken(PccAsmParser::EQUALS, 0);
}

tree::TerminalNode* PccAsmParser::AssignStmtContext::IDENT() {
  return getToken(PccAsmParser::IDENT, 0);
}

PccAsmParser::ExprContext* PccAsmParser::AssignStmtContext::expr() {
  return getRuleContext<PccAsmParser::ExprContext>(0);
}

PccAsmParser::AssignStmtContext::AssignStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any PccAsmParser::AssignStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitAssignStmt(this);
  else
    return visitor->visitChildren(this);
}
//----------------- InstrStmtContext ------------------------------------------------------------------

tree::TerminalNode* PccAsmParser::InstrStmtContext::IDENT() {
  return getToken(PccAsmParser::IDENT, 0);
}

tree::TerminalNode* PccAsmParser::InstrStmtContext::NUMBER() {
  return getToken(PccAsmParser::NUMBER, 0);
}

std::vector<PccAsmParser::OperandContext *> PccAsmParser::InstrStmtContext::operand() {
  return getRuleContexts<PccAsmParser::OperandContext>();
}

PccAsmParser::OperandContext* PccAsmParser::InstrStmtContext::operand(size_t i) {
  return getRuleContext<PccAsmParser::OperandContext>(i);
}

std::vector<tree::TerminalNode *> PccAsmParser::InstrStmtContext::COMMA() {
  return getTokens(PccAsmParser::COMMA);
}

tree::TerminalNode* PccAsmParser::InstrStmtContext::COMMA(size_t i) {
  return getToken(PccAsmParser::COMMA, i);
}

PccAsmParser::InstrStmtContext::InstrStmtContext(StatementContext *ctx) { copyFrom(ctx); }


std::any PccAsmParser::InstrStmtContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitInstrStmt(this);
  else
    return visitor->visitChildren(this);
}
PccAsmParser::StatementContext* PccAsmParser::statement() {
  StatementContext *_localctx = _tracker.createInstance<StatementContext>(_ctx, getState());
  enterRule(_localctx, 6, PccAsmParser::RuleStatement);
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
      _localctx = _tracker.createInstance<PccAsmParser::AssignStmtContext>(_localctx);
      enterOuterAlt(_localctx, 1);
      setState(40);
      antlrcpp::downCast<AssignStmtContext *>(_localctx)->lhs = match(PccAsmParser::IDENT);
      setState(41);
      match(PccAsmParser::EQUALS);
      setState(42);
      antlrcpp::downCast<AssignStmtContext *>(_localctx)->rhs = expr();
      break;
    }

    case 2: {
      _localctx = _tracker.createInstance<PccAsmParser::TimesStmtContext>(_localctx);
      enterOuterAlt(_localctx, 2);
      setState(43);
      match(PccAsmParser::T__0);
      setState(44);
      match(PccAsmParser::LPAREN);
      setState(45);
      antlrcpp::downCast<TimesStmtContext *>(_localctx)->count = expr();
      setState(46);
      match(PccAsmParser::RPAREN);
      setState(47);
      antlrcpp::downCast<TimesStmtContext *>(_localctx)->mnemonic = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == PccAsmParser::IDENT

      || _la == PccAsmParser::NUMBER)) {
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
        while (_la == PccAsmParser::COMMA) {
          setState(49);
          match(PccAsmParser::COMMA);
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
      _localctx = _tracker.createInstance<PccAsmParser::InstrStmtContext>(_localctx);
      enterOuterAlt(_localctx, 3);
      setState(58);
      antlrcpp::downCast<InstrStmtContext *>(_localctx)->mnemonic = _input->LT(1);
      _la = _input->LA(1);
      if (!(_la == PccAsmParser::IDENT

      || _la == PccAsmParser::NUMBER)) {
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
        while (_la == PccAsmParser::COMMA) {
          setState(60);
          match(PccAsmParser::COMMA);
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

PccAsmParser::OperandContext::OperandContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccAsmParser::OperandContext::LBRACKET() {
  return getToken(PccAsmParser::LBRACKET, 0);
}

PccAsmParser::ExprContext* PccAsmParser::OperandContext::expr() {
  return getRuleContext<PccAsmParser::ExprContext>(0);
}

tree::TerminalNode* PccAsmParser::OperandContext::RBRACKET() {
  return getToken(PccAsmParser::RBRACKET, 0);
}


size_t PccAsmParser::OperandContext::getRuleIndex() const {
  return PccAsmParser::RuleOperand;
}


std::any PccAsmParser::OperandContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitOperand(this);
  else
    return visitor->visitChildren(this);
}

PccAsmParser::OperandContext* PccAsmParser::operand() {
  OperandContext *_localctx = _tracker.createInstance<OperandContext>(_ctx, getState());
  enterRule(_localctx, 8, PccAsmParser::RuleOperand);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(76);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccAsmParser::LBRACKET: {
        enterOuterAlt(_localctx, 1);
        setState(71);
        match(PccAsmParser::LBRACKET);
        setState(72);
        expr();
        setState(73);
        match(PccAsmParser::RBRACKET);
        break;
      }

      case PccAsmParser::IDENT:
      case PccAsmParser::NUMBER:
      case PccAsmParser::PLUS:
      case PccAsmParser::MINUS:
      case PccAsmParser::LPAREN: {
        enterOuterAlt(_localctx, 2);
        setState(75);
        expr();
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

//----------------- ExprContext ------------------------------------------------------------------

PccAsmParser::ExprContext::ExprContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<PccAsmParser::SyllableContext *> PccAsmParser::ExprContext::syllable() {
  return getRuleContexts<PccAsmParser::SyllableContext>();
}

PccAsmParser::SyllableContext* PccAsmParser::ExprContext::syllable(size_t i) {
  return getRuleContext<PccAsmParser::SyllableContext>(i);
}


size_t PccAsmParser::ExprContext::getRuleIndex() const {
  return PccAsmParser::RuleExpr;
}


std::any PccAsmParser::ExprContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitExpr(this);
  else
    return visitor->visitChildren(this);
}

PccAsmParser::ExprContext* PccAsmParser::expr() {
  ExprContext *_localctx = _tracker.createInstance<ExprContext>(_ctx, getState());
  enterRule(_localctx, 10, PccAsmParser::RuleExpr);
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
    setState(78);
    syllable();
    setState(82);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2444) != 0)) {
      setState(79);
      syllable();
      setState(84);
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

PccAsmParser::SyllableContext::SyllableContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

PccAsmParser::AtomContext* PccAsmParser::SyllableContext::atom() {
  return getRuleContext<PccAsmParser::AtomContext>(0);
}

PccAsmParser::SignContext* PccAsmParser::SyllableContext::sign() {
  return getRuleContext<PccAsmParser::SignContext>(0);
}


size_t PccAsmParser::SyllableContext::getRuleIndex() const {
  return PccAsmParser::RuleSyllable;
}


std::any PccAsmParser::SyllableContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitSyllable(this);
  else
    return visitor->visitChildren(this);
}

PccAsmParser::SyllableContext* PccAsmParser::syllable() {
  SyllableContext *_localctx = _tracker.createInstance<SyllableContext>(_ctx, getState());
  enterRule(_localctx, 12, PccAsmParser::RuleSyllable);
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
    setState(86);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == PccAsmParser::PLUS

    || _la == PccAsmParser::MINUS) {
      setState(85);
      sign();
    }
    setState(88);
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

PccAsmParser::SignContext::SignContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccAsmParser::SignContext::PLUS() {
  return getToken(PccAsmParser::PLUS, 0);
}

tree::TerminalNode* PccAsmParser::SignContext::MINUS() {
  return getToken(PccAsmParser::MINUS, 0);
}


size_t PccAsmParser::SignContext::getRuleIndex() const {
  return PccAsmParser::RuleSign;
}


std::any PccAsmParser::SignContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitSign(this);
  else
    return visitor->visitChildren(this);
}

PccAsmParser::SignContext* PccAsmParser::sign() {
  SignContext *_localctx = _tracker.createInstance<SignContext>(_ctx, getState());
  enterRule(_localctx, 14, PccAsmParser::RuleSign);
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
    _la = _input->LA(1);
    if (!(_la == PccAsmParser::PLUS

    || _la == PccAsmParser::MINUS)) {
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

PccAsmParser::AtomContext::AtomContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* PccAsmParser::AtomContext::IDENT() {
  return getToken(PccAsmParser::IDENT, 0);
}

tree::TerminalNode* PccAsmParser::AtomContext::NUMBER() {
  return getToken(PccAsmParser::NUMBER, 0);
}

tree::TerminalNode* PccAsmParser::AtomContext::LPAREN() {
  return getToken(PccAsmParser::LPAREN, 0);
}

PccAsmParser::ExprContext* PccAsmParser::AtomContext::expr() {
  return getRuleContext<PccAsmParser::ExprContext>(0);
}

tree::TerminalNode* PccAsmParser::AtomContext::RPAREN() {
  return getToken(PccAsmParser::RPAREN, 0);
}


size_t PccAsmParser::AtomContext::getRuleIndex() const {
  return PccAsmParser::RuleAtom;
}


std::any PccAsmParser::AtomContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<PccAsmVisitor*>(visitor))
    return parserVisitor->visitAtom(this);
  else
    return visitor->visitChildren(this);
}

PccAsmParser::AtomContext* PccAsmParser::atom() {
  AtomContext *_localctx = _tracker.createInstance<AtomContext>(_ctx, getState());
  enterRule(_localctx, 16, PccAsmParser::RuleAtom);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(98);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case PccAsmParser::IDENT: {
        enterOuterAlt(_localctx, 1);
        setState(92);
        match(PccAsmParser::IDENT);
        break;
      }

      case PccAsmParser::NUMBER: {
        enterOuterAlt(_localctx, 2);
        setState(93);
        match(PccAsmParser::NUMBER);
        break;
      }

      case PccAsmParser::LPAREN: {
        enterOuterAlt(_localctx, 3);
        setState(94);
        match(PccAsmParser::LPAREN);
        setState(95);
        expr();
        setState(96);
        match(PccAsmParser::RPAREN);
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

void PccAsmParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  pccasmParserInitialize();
#else
  ::antlr4::internal::call_once(pccasmParserOnceFlag, pccasmParserInitialize);
#endif
}
