#ifndef SNIFFERCOMMIT_PRESENTATION_INTERACTIVE_INIT_HPP
#define SNIFFERCOMMIT_PRESENTATION_INTERACTIVE_INIT_HPP

#include "sniffercommit/application/init_use_case.hpp"

namespace sniffercommit::presentation {

void run_interactive_init(application::InitOptions& opts);
void print_init_summary(const application::InitOptions& opts,
                        const application::InitResult& result);

}  // namespace sniffercommit::presentation

#endif
