
// Generated from /Users/tom/projects/pcc/grammar/PccAsm.g4 by ANTLR 4.13.2


#include "PccAsmLexer.h"


using namespace antlr4;



using namespace antlr4;

namespace {

struct PccAsmLexerStaticData final {
  PccAsmLexerStaticData(std::vector<std::string> ruleNames,
                          std::vector<std::string> channelNames,
                          std::vector<std::string> modeNames,
                          std::vector<std::string> literalNames,
                          std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), channelNames(std::move(channelNames)),
        modeNames(std::move(modeNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  PccAsmLexerStaticData(const PccAsmLexerStaticData&) = delete;
  PccAsmLexerStaticData(PccAsmLexerStaticData&&) = delete;
  PccAsmLexerStaticData& operator=(const PccAsmLexerStaticData&) = delete;
  PccAsmLexerStaticData& operator=(PccAsmLexerStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> channelNames;
  const std::vector<std::string> modeNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag pccasmlexerLexerOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<PccAsmLexerStaticData> pccasmlexerLexerStaticData = nullptr;

void pccasmlexerLexerInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (pccasmlexerLexerStaticData != nullptr) {
    return;
  }
#else
  assert(pccasmlexerLexerStaticData == nullptr);
#endif
  auto staticData = std::make_unique<PccAsmLexerStaticData>(
    std::vector<std::string>{
      "T__0", "IDENT", "NUMBER", "COLON", "EQUALS", "COMMA", "PLUS", "MINUS", 
      "LBRACKET", "RBRACKET", "LPAREN", "RPAREN", "COMMENT", "NEWLINE", 
      "WS"
    },
    std::vector<std::string>{
      "DEFAULT_TOKEN_CHANNEL", "HIDDEN"
    },
    std::vector<std::string>{
      "DEFAULT_MODE"
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
  	4,0,15,98,6,-1,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,
  	6,2,7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,
  	7,14,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,1,5,1,40,8,1,10,1,12,1,43,9,1,1,2,
  	1,2,1,2,1,2,4,2,49,8,2,11,2,12,2,50,1,2,4,2,54,8,2,11,2,12,2,55,3,2,58,
  	8,2,1,3,1,3,1,4,1,4,1,5,1,5,1,6,1,6,1,7,1,7,1,8,1,8,1,9,1,9,1,10,1,10,
  	1,11,1,11,1,12,1,12,5,12,80,8,12,10,12,12,12,83,9,12,1,12,1,12,1,13,3,
  	13,88,8,13,1,13,1,13,1,14,4,14,93,8,14,11,14,12,14,94,1,14,1,14,0,0,15,
  	1,1,3,2,5,3,7,4,9,5,11,6,13,7,15,8,17,9,19,10,21,11,23,12,25,13,27,14,
  	29,15,1,0,6,5,0,36,36,46,46,65,90,95,95,97,122,6,0,36,36,46,46,48,57,
  	65,90,95,95,97,122,3,0,48,57,65,70,97,102,1,0,48,57,2,0,10,10,13,13,2,
  	0,9,9,32,32,104,0,1,1,0,0,0,0,3,1,0,0,0,0,5,1,0,0,0,0,7,1,0,0,0,0,9,1,
  	0,0,0,0,11,1,0,0,0,0,13,1,0,0,0,0,15,1,0,0,0,0,17,1,0,0,0,0,19,1,0,0,
  	0,0,21,1,0,0,0,0,23,1,0,0,0,0,25,1,0,0,0,0,27,1,0,0,0,0,29,1,0,0,0,1,
  	31,1,0,0,0,3,37,1,0,0,0,5,57,1,0,0,0,7,59,1,0,0,0,9,61,1,0,0,0,11,63,
  	1,0,0,0,13,65,1,0,0,0,15,67,1,0,0,0,17,69,1,0,0,0,19,71,1,0,0,0,21,73,
  	1,0,0,0,23,75,1,0,0,0,25,77,1,0,0,0,27,87,1,0,0,0,29,92,1,0,0,0,31,32,
  	5,116,0,0,32,33,5,105,0,0,33,34,5,109,0,0,34,35,5,101,0,0,35,36,5,115,
  	0,0,36,2,1,0,0,0,37,41,7,0,0,0,38,40,7,1,0,0,39,38,1,0,0,0,40,43,1,0,
  	0,0,41,39,1,0,0,0,41,42,1,0,0,0,42,4,1,0,0,0,43,41,1,0,0,0,44,45,5,48,
  	0,0,45,46,5,120,0,0,46,48,1,0,0,0,47,49,7,2,0,0,48,47,1,0,0,0,49,50,1,
  	0,0,0,50,48,1,0,0,0,50,51,1,0,0,0,51,58,1,0,0,0,52,54,7,3,0,0,53,52,1,
  	0,0,0,54,55,1,0,0,0,55,53,1,0,0,0,55,56,1,0,0,0,56,58,1,0,0,0,57,44,1,
  	0,0,0,57,53,1,0,0,0,58,6,1,0,0,0,59,60,5,58,0,0,60,8,1,0,0,0,61,62,5,
  	61,0,0,62,10,1,0,0,0,63,64,5,44,0,0,64,12,1,0,0,0,65,66,5,43,0,0,66,14,
  	1,0,0,0,67,68,5,45,0,0,68,16,1,0,0,0,69,70,5,91,0,0,70,18,1,0,0,0,71,
  	72,5,93,0,0,72,20,1,0,0,0,73,74,5,40,0,0,74,22,1,0,0,0,75,76,5,41,0,0,
  	76,24,1,0,0,0,77,81,5,59,0,0,78,80,8,4,0,0,79,78,1,0,0,0,80,83,1,0,0,
  	0,81,79,1,0,0,0,81,82,1,0,0,0,82,84,1,0,0,0,83,81,1,0,0,0,84,85,6,12,
  	0,0,85,26,1,0,0,0,86,88,5,13,0,0,87,86,1,0,0,0,87,88,1,0,0,0,88,89,1,
  	0,0,0,89,90,5,10,0,0,90,28,1,0,0,0,91,93,7,5,0,0,92,91,1,0,0,0,93,94,
  	1,0,0,0,94,92,1,0,0,0,94,95,1,0,0,0,95,96,1,0,0,0,96,97,6,14,0,0,97,30,
  	1,0,0,0,8,0,41,50,55,57,81,87,94,1,6,0,0
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  pccasmlexerLexerStaticData = std::move(staticData);
}

}

PccAsmLexer::PccAsmLexer(CharStream *input) : Lexer(input) {
  PccAsmLexer::initialize();
  _interpreter = new atn::LexerATNSimulator(this, *pccasmlexerLexerStaticData->atn, pccasmlexerLexerStaticData->decisionToDFA, pccasmlexerLexerStaticData->sharedContextCache);
}

PccAsmLexer::~PccAsmLexer() {
  delete _interpreter;
}

std::string PccAsmLexer::getGrammarFileName() const {
  return "PccAsm.g4";
}

const std::vector<std::string>& PccAsmLexer::getRuleNames() const {
  return pccasmlexerLexerStaticData->ruleNames;
}

const std::vector<std::string>& PccAsmLexer::getChannelNames() const {
  return pccasmlexerLexerStaticData->channelNames;
}

const std::vector<std::string>& PccAsmLexer::getModeNames() const {
  return pccasmlexerLexerStaticData->modeNames;
}

const dfa::Vocabulary& PccAsmLexer::getVocabulary() const {
  return pccasmlexerLexerStaticData->vocabulary;
}

antlr4::atn::SerializedATNView PccAsmLexer::getSerializedATN() const {
  return pccasmlexerLexerStaticData->serializedATN;
}

const atn::ATN& PccAsmLexer::getATN() const {
  return *pccasmlexerLexerStaticData->atn;
}




void PccAsmLexer::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  pccasmlexerLexerInitialize();
#else
  ::antlr4::internal::call_once(pccasmlexerLexerOnceFlag, pccasmlexerLexerInitialize);
#endif
}
