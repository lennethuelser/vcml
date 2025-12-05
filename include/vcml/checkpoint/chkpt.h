#ifndef VCML_CHECKPOINT_CHKPT_H
#define VCML_CHECKPOINT_CHKPT_H

#include <string>
#include "vcml/core/types.h"
#include "vcml/core/systemc.h"
#include "vcml/core/peripheral.h"
#include "vcml/core/model.h"

#include "vcml/protocols/tlm.h"

namespace vcml {
namespace checkpoint {

class chkpt //: public peripheral
{
public:
    enum LOG_LVL {
        LOG_MSG = 0,   // Print messages regardless of log level
        LOG_ERROR = 1, // Errors only
        LOG_WARN = 2,  // Warnings
        LOG_INFO = 3,  // Informative
        LOG_DEBUG = 4  // Debug only
    };

    chkpt();//const sc_module_name& nm);
    chkpt(const chkpt&) = delete;
    
    static chkpt s_chkpt;
    static chkpt& instance();

    void checkpoint();
    const std::string& get_img_dir() const;

    //reg<u32> waitc;
    //tlm_target_socket in;
    //VCML_KIND(chkpt);

    //virtual void reset() override;
private:


    std::string m_socket_path;
    enum LOG_LVL m_log_lvl;
    std::string m_log_file_path;
    std::string m_img_dir;
    int m_img_dir_fd;
    bool m_initialized;

    bool is_error(int retval) const;
    void init();
};

} // namespace checkpoint
} // namespace vcml

#endif
