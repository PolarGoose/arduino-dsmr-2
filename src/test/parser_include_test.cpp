// This code tests that the parser has all necessary dependencies included in its headers.
// We check that the code compiles.

#include "dsmr/fields.h"
#include "dsmr/parser.h"

using namespace arduino_dsmr_2;
using namespace fields;

void P1Parser_some_function() {
  const auto& msg = "";
  ParsedData<identification, p1_version> data;
  P1Parser::parse(&data, msg, std::size(msg), true);
}
