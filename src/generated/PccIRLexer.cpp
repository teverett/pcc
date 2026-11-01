
// Generated from /Users/tom/projects/pcc/grammar/PccIRLexer.g4 by ANTLR 4.13.2


#include "PccIRLexer.h"


using namespace antlr4;



using namespace antlr4;

namespace {

struct PccIRLexerStaticData final {
  PccIRLexerStaticData(std::vector<std::string> ruleNames,
                          std::vector<std::string> channelNames,
                          std::vector<std::string> modeNames,
                          std::vector<std::string> literalNames,
                          std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), channelNames(std::move(channelNames)),
        modeNames(std::move(modeNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  PccIRLexerStaticData(const PccIRLexerStaticData&) = delete;
  PccIRLexerStaticData(PccIRLexerStaticData&&) = delete;
  PccIRLexerStaticData& operator=(const PccIRLexerStaticData&) = delete;
  PccIRLexerStaticData& operator=(PccIRLexerStaticData&&) = delete;

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

::antlr4::internal::OnceFlag pccirlexerLexerOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<PccIRLexerStaticData> pccirlexerLexerStaticData = nullptr;

void pccirlexerLexerInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (pccirlexerLexerStaticData != nullptr) {
    return;
  }
#else
  assert(pccirlexerLexerStaticData == nullptr);
#endif
  auto staticData = std::make_unique<PccIRLexerStaticData>(
    std::vector<std::string>{
      "GLOBALS_HEADER", "FUNCTIONS_HEADER", "INT_KW", "LOCAL_KW", "IF_KW", 
      "ELSE_KW", "WHILE_KW", "RETURN_KW", "LPAREN", "RPAREN", "LBRACK", 
      "RBRACK", "LBRACE", "RBRACE", "SEMI", "COMMA", "ASSIGN", "HASH", "STAR", 
      "AMP", "MINUS", "BANG", "TILDE", "SLASH", "PERCENT", "PLUS", "SHL", 
      "SHR", "LE", "GE", "LT", "GT", "EQ", "NE", "CARET", "PIPE", "ANDAND", 
      "OROR", "IDENT", "NUMBER", "WS", "ASM_BEGIN", "ASM_TEXT", "ASM_END"
    },
    std::vector<std::string>{
      "DEFAULT_TOKEN_CHANNEL", "HIDDEN"
    },
    std::vector<std::string>{
      "DEFAULT_MODE", "ASM_MODE"
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
  	4,0,44,272,6,-1,6,-1,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,
  	2,6,7,6,2,7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,
  	13,2,14,7,14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,
  	20,2,21,7,21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,
  	27,2,28,7,28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,
  	34,2,35,7,35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,
  	41,2,42,7,42,2,43,7,43,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,
  	0,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,2,1,2,1,2,1,3,1,3,1,3,1,3,1,
  	3,1,3,1,4,1,4,1,4,1,5,1,5,1,5,1,5,1,5,1,6,1,6,1,6,1,6,1,6,1,6,1,7,1,7,
  	1,7,1,7,1,7,1,7,1,7,1,8,1,8,1,9,1,9,1,10,1,10,1,11,1,11,1,12,1,12,1,13,
  	1,13,1,14,1,14,1,15,1,15,1,16,1,16,1,17,1,17,1,18,1,18,1,19,1,19,1,20,
  	1,20,1,21,1,21,1,22,1,22,1,23,1,23,1,24,1,24,1,25,1,25,1,26,1,26,1,26,
  	1,27,1,27,1,27,1,28,1,28,1,28,1,29,1,29,1,29,1,30,1,30,1,31,1,31,1,32,
  	1,32,1,32,1,33,1,33,1,33,1,34,1,34,1,35,1,35,1,36,1,36,1,36,1,37,1,37,
  	1,37,1,38,1,38,5,38,230,8,38,10,38,12,38,233,9,38,1,39,3,39,236,8,39,
  	1,39,4,39,239,8,39,11,39,12,39,240,1,40,4,40,244,8,40,11,40,12,40,245,
  	1,40,1,40,1,41,1,41,1,41,1,41,1,41,5,41,255,8,41,10,41,12,41,258,9,41,
  	1,41,1,41,1,41,1,41,1,42,4,42,265,8,42,11,42,12,42,266,1,43,1,43,1,43,
  	1,43,0,0,44,2,1,4,2,6,3,8,4,10,5,12,6,14,7,16,8,18,9,20,10,22,11,24,12,
  	26,13,28,14,30,15,32,16,34,17,36,18,38,19,40,20,42,21,44,22,46,23,48,
  	24,50,25,52,26,54,27,56,28,58,29,60,30,62,31,64,32,66,33,68,34,70,35,
  	72,36,74,37,76,38,78,39,80,40,82,41,84,42,86,43,88,44,2,0,1,5,3,0,65,
  	90,95,95,97,122,4,0,48,57,65,90,95,95,97,122,1,0,48,57,3,0,9,10,13,13,
  	32,32,1,0,125,125,276,0,2,1,0,0,0,0,4,1,0,0,0,0,6,1,0,0,0,0,8,1,0,0,0,
  	0,10,1,0,0,0,0,12,1,0,0,0,0,14,1,0,0,0,0,16,1,0,0,0,0,18,1,0,0,0,0,20,
  	1,0,0,0,0,22,1,0,0,0,0,24,1,0,0,0,0,26,1,0,0,0,0,28,1,0,0,0,0,30,1,0,
  	0,0,0,32,1,0,0,0,0,34,1,0,0,0,0,36,1,0,0,0,0,38,1,0,0,0,0,40,1,0,0,0,
  	0,42,1,0,0,0,0,44,1,0,0,0,0,46,1,0,0,0,0,48,1,0,0,0,0,50,1,0,0,0,0,52,
  	1,0,0,0,0,54,1,0,0,0,0,56,1,0,0,0,0,58,1,0,0,0,0,60,1,0,0,0,0,62,1,0,
  	0,0,0,64,1,0,0,0,0,66,1,0,0,0,0,68,1,0,0,0,0,70,1,0,0,0,0,72,1,0,0,0,
  	0,74,1,0,0,0,0,76,1,0,0,0,0,78,1,0,0,0,0,80,1,0,0,0,0,82,1,0,0,0,0,84,
  	1,0,0,0,1,86,1,0,0,0,1,88,1,0,0,0,2,90,1,0,0,0,4,108,1,0,0,0,6,128,1,
  	0,0,0,8,132,1,0,0,0,10,138,1,0,0,0,12,141,1,0,0,0,14,146,1,0,0,0,16,152,
  	1,0,0,0,18,159,1,0,0,0,20,161,1,0,0,0,22,163,1,0,0,0,24,165,1,0,0,0,26,
  	167,1,0,0,0,28,169,1,0,0,0,30,171,1,0,0,0,32,173,1,0,0,0,34,175,1,0,0,
  	0,36,177,1,0,0,0,38,179,1,0,0,0,40,181,1,0,0,0,42,183,1,0,0,0,44,185,
  	1,0,0,0,46,187,1,0,0,0,48,189,1,0,0,0,50,191,1,0,0,0,52,193,1,0,0,0,54,
  	195,1,0,0,0,56,198,1,0,0,0,58,201,1,0,0,0,60,204,1,0,0,0,62,207,1,0,0,
  	0,64,209,1,0,0,0,66,211,1,0,0,0,68,214,1,0,0,0,70,217,1,0,0,0,72,219,
  	1,0,0,0,74,221,1,0,0,0,76,224,1,0,0,0,78,227,1,0,0,0,80,235,1,0,0,0,82,
  	243,1,0,0,0,84,249,1,0,0,0,86,264,1,0,0,0,88,268,1,0,0,0,90,91,5,59,0,
  	0,91,92,5,32,0,0,92,93,5,45,0,0,93,94,5,45,0,0,94,95,5,45,0,0,95,96,5,
  	32,0,0,96,97,5,103,0,0,97,98,5,108,0,0,98,99,5,111,0,0,99,100,5,98,0,
  	0,100,101,5,97,0,0,101,102,5,108,0,0,102,103,5,115,0,0,103,104,5,32,0,
  	0,104,105,5,45,0,0,105,106,5,45,0,0,106,107,5,45,0,0,107,3,1,0,0,0,108,
  	109,5,59,0,0,109,110,5,32,0,0,110,111,5,45,0,0,111,112,5,45,0,0,112,113,
  	5,45,0,0,113,114,5,32,0,0,114,115,5,102,0,0,115,116,5,117,0,0,116,117,
  	5,110,0,0,117,118,5,99,0,0,118,119,5,116,0,0,119,120,5,105,0,0,120,121,
  	5,111,0,0,121,122,5,110,0,0,122,123,5,115,0,0,123,124,5,32,0,0,124,125,
  	5,45,0,0,125,126,5,45,0,0,126,127,5,45,0,0,127,5,1,0,0,0,128,129,5,105,
  	0,0,129,130,5,110,0,0,130,131,5,116,0,0,131,7,1,0,0,0,132,133,5,108,0,
  	0,133,134,5,111,0,0,134,135,5,99,0,0,135,136,5,97,0,0,136,137,5,108,0,
  	0,137,9,1,0,0,0,138,139,5,105,0,0,139,140,5,102,0,0,140,11,1,0,0,0,141,
  	142,5,101,0,0,142,143,5,108,0,0,143,144,5,115,0,0,144,145,5,101,0,0,145,
  	13,1,0,0,0,146,147,5,119,0,0,147,148,5,104,0,0,148,149,5,105,0,0,149,
  	150,5,108,0,0,150,151,5,101,0,0,151,15,1,0,0,0,152,153,5,114,0,0,153,
  	154,5,101,0,0,154,155,5,116,0,0,155,156,5,117,0,0,156,157,5,114,0,0,157,
  	158,5,110,0,0,158,17,1,0,0,0,159,160,5,40,0,0,160,19,1,0,0,0,161,162,
  	5,41,0,0,162,21,1,0,0,0,163,164,5,91,0,0,164,23,1,0,0,0,165,166,5,93,
  	0,0,166,25,1,0,0,0,167,168,5,123,0,0,168,27,1,0,0,0,169,170,5,125,0,0,
  	170,29,1,0,0,0,171,172,5,59,0,0,172,31,1,0,0,0,173,174,5,44,0,0,174,33,
  	1,0,0,0,175,176,5,61,0,0,176,35,1,0,0,0,177,178,5,35,0,0,178,37,1,0,0,
  	0,179,180,5,42,0,0,180,39,1,0,0,0,181,182,5,38,0,0,182,41,1,0,0,0,183,
  	184,5,45,0,0,184,43,1,0,0,0,185,186,5,33,0,0,186,45,1,0,0,0,187,188,5,
  	126,0,0,188,47,1,0,0,0,189,190,5,47,0,0,190,49,1,0,0,0,191,192,5,37,0,
  	0,192,51,1,0,0,0,193,194,5,43,0,0,194,53,1,0,0,0,195,196,5,60,0,0,196,
  	197,5,60,0,0,197,55,1,0,0,0,198,199,5,62,0,0,199,200,5,62,0,0,200,57,
  	1,0,0,0,201,202,5,60,0,0,202,203,5,61,0,0,203,59,1,0,0,0,204,205,5,62,
  	0,0,205,206,5,61,0,0,206,61,1,0,0,0,207,208,5,60,0,0,208,63,1,0,0,0,209,
  	210,5,62,0,0,210,65,1,0,0,0,211,212,5,61,0,0,212,213,5,61,0,0,213,67,
  	1,0,0,0,214,215,5,33,0,0,215,216,5,61,0,0,216,69,1,0,0,0,217,218,5,94,
  	0,0,218,71,1,0,0,0,219,220,5,124,0,0,220,73,1,0,0,0,221,222,5,38,0,0,
  	222,223,5,38,0,0,223,75,1,0,0,0,224,225,5,124,0,0,225,226,5,124,0,0,226,
  	77,1,0,0,0,227,231,7,0,0,0,228,230,7,1,0,0,229,228,1,0,0,0,230,233,1,
  	0,0,0,231,229,1,0,0,0,231,232,1,0,0,0,232,79,1,0,0,0,233,231,1,0,0,0,
  	234,236,5,45,0,0,235,234,1,0,0,0,235,236,1,0,0,0,236,238,1,0,0,0,237,
  	239,7,2,0,0,238,237,1,0,0,0,239,240,1,0,0,0,240,238,1,0,0,0,240,241,1,
  	0,0,0,241,81,1,0,0,0,242,244,7,3,0,0,243,242,1,0,0,0,244,245,1,0,0,0,
  	245,243,1,0,0,0,245,246,1,0,0,0,246,247,1,0,0,0,247,248,6,40,0,0,248,
  	83,1,0,0,0,249,250,5,97,0,0,250,251,5,115,0,0,251,252,5,109,0,0,252,256,
  	1,0,0,0,253,255,7,3,0,0,254,253,1,0,0,0,255,258,1,0,0,0,256,254,1,0,0,
  	0,256,257,1,0,0,0,257,259,1,0,0,0,258,256,1,0,0,0,259,260,5,123,0,0,260,
  	261,1,0,0,0,261,262,6,41,1,0,262,85,1,0,0,0,263,265,8,4,0,0,264,263,1,
  	0,0,0,265,266,1,0,0,0,266,264,1,0,0,0,266,267,1,0,0,0,267,87,1,0,0,0,
  	268,269,5,125,0,0,269,270,1,0,0,0,270,271,6,43,2,0,271,89,1,0,0,0,8,0,
  	1,231,235,240,245,256,266,3,6,0,0,5,1,0,4,0,0
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  pccirlexerLexerStaticData = std::move(staticData);
}

}

PccIRLexer::PccIRLexer(CharStream *input) : Lexer(input) {
  PccIRLexer::initialize();
  _interpreter = new atn::LexerATNSimulator(this, *pccirlexerLexerStaticData->atn, pccirlexerLexerStaticData->decisionToDFA, pccirlexerLexerStaticData->sharedContextCache);
}

PccIRLexer::~PccIRLexer() {
  delete _interpreter;
}

std::string PccIRLexer::getGrammarFileName() const {
  return "PccIRLexer.g4";
}

const std::vector<std::string>& PccIRLexer::getRuleNames() const {
  return pccirlexerLexerStaticData->ruleNames;
}

const std::vector<std::string>& PccIRLexer::getChannelNames() const {
  return pccirlexerLexerStaticData->channelNames;
}

const std::vector<std::string>& PccIRLexer::getModeNames() const {
  return pccirlexerLexerStaticData->modeNames;
}

const dfa::Vocabulary& PccIRLexer::getVocabulary() const {
  return pccirlexerLexerStaticData->vocabulary;
}

antlr4::atn::SerializedATNView PccIRLexer::getSerializedATN() const {
  return pccirlexerLexerStaticData->serializedATN;
}

const atn::ATN& PccIRLexer::getATN() const {
  return *pccirlexerLexerStaticData->atn;
}




void PccIRLexer::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  pccirlexerLexerInitialize();
#else
  ::antlr4::internal::call_once(pccirlexerLexerOnceFlag, pccirlexerLexerInitialize);
#endif
}
