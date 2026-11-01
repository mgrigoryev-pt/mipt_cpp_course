#include "field_traits.h"

namespace nano_edr {

bool FieldTraits<std::string>::Parse(const std::string&, std::string*) {
    return false;
}
bool FieldTraits<uint64_t>::Parse(const std::string&, uint64_t*) {
    return false;
}
bool FieldTraits<bool>::Parse(const std::string&, bool*) {
    return false;
}

}  // namespace nano_edr
