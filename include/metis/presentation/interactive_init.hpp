#ifndef METIS_PRESENTATION_INTERACTIVE_INIT_HPP
#define METIS_PRESENTATION_INTERACTIVE_INIT_HPP

#include "metis/application/init_use_case.hpp"

namespace metis::presentation {

void run_interactive_init(application::InitOptions& opts);
void print_init_summary(const application::InitOptions& opts,
                        const application::InitResult& result);
}  // namespace metis::presentation

#endif
