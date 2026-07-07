#ifndef CONTENT_STARTUP_VALIDATOR_H
#define CONTENT_STARTUP_VALIDATOR_H

namespace defn {

class ContentStartupValidator {
  public:
    ContentStartupValidator() = delete;

    static bool report_startup_validation();
};

} // namespace defn

#endif