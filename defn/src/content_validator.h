#ifndef CONTENT_VALIDATOR_H
#define CONTENT_VALIDATOR_H

namespace defn {

class ContentValidator {
  public:
    ContentValidator() = delete;

    static bool report_startup_validation();
};

} // namespace defn

#endif