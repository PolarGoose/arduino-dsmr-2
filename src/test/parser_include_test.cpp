// This code tests that the parser has all necessary dependencies included in its headers.
// We check that the code compiles.

#include "dsmr/fields.h"
#include "dsmr/parser.h"

using namespace dsmr;
using namespace fields;

void SomeFunction() {
  const auto& msg = "";
  ParsedData<identification, p1_version> data;
  const auto& res = P1Parser::parse(&data, msg, std::size(msg), true);
}
